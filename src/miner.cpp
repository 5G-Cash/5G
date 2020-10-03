// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "main.h"
#include "base58.h"
#include "net.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"
#include "wallet/wallet.h"
#include "definition.h"
#include "crypto/scrypt.h"
#include "fivegnode-payments.h"
#include "fivegnode-sync.h"
#include "fivegnodeman.h"
#include "zerocoin.h"
#include "sigma.h"
#include "sigma/remint.h"
#include <algorithm>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <queue>
#include <unistd.h>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
uint64_t nLastBlockWeight = 0;
int64_t nLastCoinStakeSearchInterval = 0;
unsigned int nMinerSleep = 4000;
class ScoreCompare
{
public:
    ScoreCompare() {}

    bool operator()(const CTxMemPool::txiter a, const CTxMemPool::txiter b)
    {
        return CompareTxMemPoolEntryByScore()(*b,*a); // Convert to less than
    }
};

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams,false);

    return nNewTime - nOldTime;
}

BlockAssembler::BlockAssembler(const CChainParams& _chainparams) : chainparams(_chainparams)
{
    // Block resource limits
    // If neither -blockmaxsize or -blockmaxweight is given, limit to DEFAULT_BLOCK_MAX_*
    // If only one is given, only restrict the specified resource.
    // If both are given, restrict both.
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
    bool fWeightSet = false;
    if (mapArgs.count("-blockmaxweight")) {
        nBlockMaxWeight = GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
        nBlockMaxSize = MAX_BLOCK_SERIALIZED_SIZE;
        fWeightSet = true;
    }
    if (mapArgs.count("-blockmaxsize")) {
        nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
        if (!fWeightSet) {
            nBlockMaxWeight = nBlockMaxSize * WITNESS_SCALE_FACTOR;
        }
    }

    // Limit weight to between 4K and MAX_BLOCK_WEIGHT-4K for sanity:
    nBlockMaxWeight = std::max((unsigned int)4000, std::min((unsigned int)(MAX_BLOCK_WEIGHT-4000), nBlockMaxWeight));
    // Limit size to between 1K and MAX_BLOCK_SERIALIZED_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SERIALIZED_SIZE-1000), nBlockMaxSize));

    // Whether we need to account for byte usage (in addition to weight usage)
    fNeedSizeAccounting = (nBlockMaxSize < MAX_BLOCK_SERIALIZED_SIZE-1000);
}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockSize = 1000;
    nBlockWeight = 4000;
    nBlockSigOpsCost = 100;
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;

    lastFewTxs = 0;
    blockFinished = false;
}

CBlockTemplate* BlockAssembler::CreateNewBlock(
    const CScript& scriptPubKeyIn,
    const vector<uint256>& tx_ids,bool fProofOfStake)
{
    // Create new block
    LogPrintf("BlockAssembler::CreateNewBlock()\n");

    const Consensus::Params &params = Params().GetConsensus();
    uint32_t nBlockTime;
    bool fMTP;
    {
        LOCK2(cs_main, mempool.cs);
        nBlockTime = GetAdjustedTime();
    }

    fMTP = false;
    int nFeeReductionFactor = 1;
    CAmount coin = COIN / nFeeReductionFactor;

    resetBlock();
    unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience
    // Create coinbase tx
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    CBlockIndex* pindexPrev = chainActive.Tip();
    const int nHeight = pindexPrev->nHeight + 1;
    if (fProofOfStake)
    {
        // Make the coinbase tx empty in case of proof of stake
        coinbaseTx.vout[0].SetEmpty();
    } else {
        coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
        coinbaseTx.vout[0].nValue = 0;
    }
    if (params.IsMain() && (GetAdjustedTime() <= nStartRewardTime)) {
        throw std::runtime_error("CreateNewBlock() : Create new block too early");
    }
    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to between 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SERIALIZED_SIZE - 1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", 0);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CTxMemPool::setEntries inBlock;
    CTxMemPool::setEntries waitSet;

    // This vector will be sorted into a priority queue:
    vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash> waitPriMap;
    typedef std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    std::priority_queue<CTxMemPool::txiter, std::vector<CTxMemPool::txiter>, ScoreCompare> clearedTxs;
    bool fPrintPriority = GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    uint64_t nBlockSize = 1500;
    uint64_t nBlockTx = 0;
    unsigned int nBlockSigOps = 100;
    int lastFewTxs = 0;
    CAmount nFees = 0;
    {
        LOCK2(cs_main, mempool.cs);
        pblock->nTime = nBlockTime;
        const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

        pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
        // -regtest only: allow overriding block.nVersion with
        // -blockversion=N to test forking scenarios
        if (chainparams.MineBlocksOnDemand())
            pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

        int64_t nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                                  ? nMedianTimePast
                                  : pblock->GetBlockTime();

        bool fPriorityBlock = nBlockPrioritySize > 0;
        if (fPriorityBlock) {
            vecPriority.reserve(mempool.mapTx.size());
            for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
                 mi != mempool.mapTx.end(); ++mi)
            {
                double dPriority = mi->GetPriority(nHeight);
                CAmount dummy;
                mempool.ApplyDeltas(mi->GetTx().GetHash(), dPriority, dummy);
                vecPriority.push_back(TxCoinAgePriority(dPriority, mi));
            }
            std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        }

        CTxMemPool::indexed_transaction_set::nth_index<3>::type::iterator mi = mempool.mapTx.get<3>().begin();
        CTxMemPool::txiter iter;
        std::size_t nSigmaSpend = 0;
        CAmount nValueSigmaSpend(0);

        while (mi != mempool.mapTx.get<3>().end() || !clearedTxs.empty())
        {
            bool priorityTx = false;
            if (fPriorityBlock && !vecPriority.empty()) { // add a tx from priority queue to fill the blockprioritysize
                priorityTx = true;
                iter = vecPriority.front().second;
                actualPriority = vecPriority.front().first;
                std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                vecPriority.pop_back();
            }
            else if (clearedTxs.empty()) { // add tx with next highest score
                iter = mempool.mapTx.project<0>(mi);
                mi++;
            }
            else {  // try to add a previously postponed child tx
                iter = clearedTxs.top();
                clearedTxs.pop();
            }

            if (inBlock.count(iter)) {
                LogPrintf("skip, due to exist!\n");
                continue; // could have been added to the priorityBlock
            }

            const CTransaction& tx = iter->GetTx();
            LogPrintf("Trying to add tx=%s\n", tx.GetHash().ToString());

            if (!tx_ids.empty() && std::find(tx_ids.begin(), tx_ids.end(), tx.GetHash()) == tx_ids.end()) {
                continue; // Skip because we were asked to include only transactions in tx_ids.
            }

            bool fOrphan = false;
            BOOST_FOREACH(CTxMemPool::txiter parent, mempool.GetMemPoolParents(iter))
            {
                if (!inBlock.count(parent)) {
                    fOrphan = true;
                    break;
                }
            }
            if (fOrphan) {
                if (priorityTx)
                    waitPriMap.insert(std::make_pair(iter,actualPriority));
                else waitSet.insert(iter);
                LogPrintf("skip tx=%s, due to fOrphan=%s\n", tx.GetHash().ToString(), fOrphan);
                continue;
            }

            unsigned int nTxSize = iter->GetTxSize();
            if (fPriorityBlock &&
                (nBlockSize + nTxSize >= nBlockPrioritySize || !AllowFree(actualPriority))) {
                fPriorityBlock = false;
                waitPriMap.clear();
            }
//            if (!priorityTx && (iter->GetModifiedFee() < ::minRelayTxFee.GetFee(nTxSize) && nBlockSize >= nBlockMinSize)) {
//                LogPrintf("skip tx=%s\n", tx.GetHash().ToString());
//                LogPrintf("iter->GetModifiedFee()=%s\n", iter->GetModifiedFee());
//                LogPrintf("::minRelayTxFee.GetFee(nTxSize)=%s\n", ::minRelayTxFee.GetFee(nTxSize));
//                LogPrintf("nBlockSize=%s\n", nBlockSize);
//                LogPrintf("nBlockMinSize=%s\n", nBlockMinSize);
//                LogPrintf("***********************************");
//                break;
//            }
            if (nBlockSize + nTxSize >= nBlockMaxSize) {
                if (nBlockSize >  nBlockMaxSize - 100 || lastFewTxs > 50) {
                    LogPrintf("stop due to size overweight", tx.GetHash().ToString());
                    LogPrintf("nBlockSize=%s\n", nBlockSize);
                    LogPrintf("nBlockMaxSize=%s\n", nBlockMaxSize);
                    break;
                }
                // Once we're within 1000 bytes of a full block, only look at 50 more txs
                // to try to fill the remaining space.
                if (nBlockSize > nBlockMaxSize - 1000) {
                    lastFewTxs++;
                }
                LogPrintf("skip tx=%s\n", tx.GetHash().ToString());
                LogPrintf("nBlockSize=%s\n", nBlockSize);
                LogPrintf("nBlockMaxSize=%s\n", nBlockMaxSize);
                continue;
            }
            if (tx.IsCoinBase()) {
                LogPrintf("skip tx=%s, coinbase tx\n", tx.GetHash().ToString());
                continue;
            }

            if (!IsFinalTx(tx, nHeight, nLockTimeCutoff)) {
                LogPrintf("skip tx=%s, not IsFinalTx\n", tx.GetHash().ToString());
                continue;
            }

            if (tx.IsSigmaMint() || tx.IsSigmaSpend()) {
                sigma::CSigmaState * sigmaState = sigma::CSigmaState::GetState();
                if(sigmaState->IsSurgeConditionDetected())
                    continue;
            }

            // temporarily disable zerocoin. Re-enable after sigma release
            // Make exception for regtest network (for remint tests)
            if (!chainparams.GetConsensus().IsRegtest() && (tx.IsZerocoinSpend() || tx.IsZerocoinMint()))
                continue;

            if(tx.IsSigmaSpend() && nHeight >= chainparams.GetConsensus().nDisableUnpaddedSigmaBlock && nHeight < chainparams.GetConsensus().nSigmaPaddingBlock)
                continue;

            if (tx.IsSigmaSpend() || tx.IsZerocoinRemint()) {
                // Sigma spend and zerocoin->sigma remint are subject to the same limits
                CAmount spendAmount = tx.IsSigmaSpend() ? sigma::GetSpendAmount(tx) : sigma::CoinRemintToV3::GetAmount(tx);

                if (tx.vin.size() > params.nMaxSigmaInputPerTransaction ||
                    spendAmount > params.nMaxValueSigmaSpendPerTransaction) {
                    continue;
                }
                if (tx.vin.size() + nSigmaSpend > params.nMaxSigmaInputPerBlock) {
                    continue;
                }
                if (spendAmount + nValueSigmaSpend > params.nMaxValueSigmaSpendPerBlock) {
                    continue;
                }

                //mempool.countZCSpend--;
                // Size limits
                unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

                LogPrintf("\n\n######################################\n");
                LogPrintf("nBlockMaxSize = %d\n", nBlockMaxSize);
                LogPrintf("nBlockSize = %d\n", nBlockSize);
                LogPrintf("nTxSize = %d\n", nTxSize);
                LogPrintf("nBlockSize + nTxSize  = %d\n", nBlockSize + nTxSize);
                LogPrintf("nBlockSigOpsCost  = %d\n", nBlockSigOpsCost);
                LogPrintf("GetLegacySigOpCount  = %d\n", GetLegacySigOpCount(tx));
                LogPrintf("######################################\n\n\n");

                if (nBlockSize + nTxSize >= nBlockMaxSize) {
                    LogPrintf("failed by sized\n");
                    continue;
                }

                // Legacy limits on sigOps:
                unsigned int nTxSigOps = GetLegacySigOpCount(tx);
                if (nBlockSigOpsCost + nTxSigOps >= MAX_BLOCK_SIGOPS_COST) {
                    LogPrintf("failed by sized\n");
                    continue;
                }

                CAmount nTxFees = iter->GetFee();

                pblock->vtx.push_back(tx);
                pblocktemplate->vTxFees.push_back(nTxFees);
                pblocktemplate->vTxSigOpsCost.push_back(nTxSigOps);
                nBlockSize += nTxSize;
                ++nBlockTx;
                nBlockSigOpsCost += nTxSigOps;
                nFees += nTxFees;
                nSigmaSpend += tx.vin.size();
                nValueSigmaSpend += spendAmount;
                inBlock.insert(iter);
                continue;
            }


            unsigned int nTxSigOps = iter->GetSigOpCost();
            LogPrintf("nTxSigOps=%s\n", nTxSigOps);
            LogPrintf("nBlockSigOps=%s\n", nBlockSigOps);
            LogPrintf("MAX_BLOCK_SIGOPS_COST=%s\n", MAX_BLOCK_SIGOPS_COST);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS_COST) {
                if (nBlockSigOps > MAX_BLOCK_SIGOPS_COST - 2) {
                    LogPrintf("stop due to cross fee\n", tx.GetHash().ToString());
                    break;
                }
                LogPrintf("skip tx=%s, nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS_COST\n", tx.GetHash().ToString());
                continue;
            }
            CAmount nTxFees = iter->GetFee();
            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOpsCost.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;
            LogPrintf("added to block=%s\n", tx.GetHash().ToString());
            if (fPrintPriority)
            {
                double dPriority = iter->GetPriority(nHeight);
                CAmount dummy;
                mempool.ApplyDeltas(tx.GetHash(), dPriority, dummy);
                LogPrintf("priority %.1f fee %s txid %s\n",
                          dPriority , CFeeRate(iter->GetModifiedFee(), nTxSize).ToString(), tx.GetHash().ToString());
            }

            inBlock.insert(iter);

            // Add transactions that depend on this one to the priority queue
            BOOST_FOREACH(CTxMemPool::txiter child, mempool.GetMemPoolChildren(iter))
            {
                if (fPriorityBlock) {
                    waitPriIter wpiter = waitPriMap.find(child);
                    if (wpiter != waitPriMap.end()) {
                        vecPriority.push_back(TxCoinAgePriority(wpiter->second,child));
                        std::push_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                        waitPriMap.erase(wpiter);
                    }
                }
                else {
                    if (waitSet.count(child)) {
                        clearedTxs.push(child);
                        waitSet.erase(child);
                    }
                }
            }
        }
        CAmount blockReward = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus(), nBlockTime);
        // Update coinbase transaction with additional info about fivegnode and governance payments,
        // get some info back to pass to getblocktemplate
        bool fPayFIVEGNode = nHeight >= chainparams.GetConsensus().nFivegnodePaymentsStartBlock && !fProofOfStake;
        CAmount fivegnodePayment = 0;
        if (fPayFIVEGNode) {
            const Consensus::Params &params = chainparams.GetConsensus();
            fivegnodePayment = GetFivegnodePayment(chainparams.GetConsensus(),false,nHeight);
            FillBlockPayments(coinbaseTx, nHeight, fivegnodePayment, pblock->txoutFivegnode, pblock->voutSuperblock);
        }
        //Only take out idx payment if a fivegnode is actually filled in txoutfivegnode and fivegnodepayment is not 0
        if(pblock->txoutFivegnode != CTxOut() && fivegnodePayment != 0)
            coinbaseTx.vout[0].nValue -= fivegnodePayment;

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u txs: %u fees: %ld sigops %d\n", nBlockSize, nBlockTx, nFees, nBlockSigOps);

        // Compute final coinbase transaction.
        if(!fProofOfStake)//Only Set vout of coinbasetx as blockreward in PoW Blocks
            coinbaseTx.vout[0].nValue += blockReward;
        coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
        pblock->vtx[0] = coinbaseTx;
        pblocktemplate->vTxFees[0] = -nFees;

        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        if(!fProofOfStake){
            UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
        }
        pblock->nBits  = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus(),fProofOfStake);
        pblock->nNonce = fProofOfStake ? 0 : 1;
        pblocktemplate->vTxSigOpsCost[0] = GetLegacySigOpCount(pblock->vtx[0]);

        //LogPrintf("CreateNewBlock(): AFTER pblocktemplate->vTxSigOpsCost[0] = GetLegacySigOpCount(pblock->vtx[0])\n");

        CValidationState state;
        //LogPrintf("CreateNewBlock(): BEFORE TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)\n");
        if ( !fProofOfStake && !TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
            throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
        }
        //LogPrintf("CreateNewBlock(): AFTER TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)\n");
    }
    //LogPrintf("CreateNewBlock(): pblocktemplate.release()\n");
    return pblocktemplate.release();
}


CBlockTemplate* BlockAssembler::CreateNewBlockWithKey(CReserveKey &reservekey) {
    LogPrintf("CreateNewBlockWithKey()\n");
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CScript scriptPubKey = CScript() << pubkey << OP_CHECKSIG;
//    CScript scriptPubKey = GetScriptForDestination(pubkey.GetID());;
    return CreateNewBlock(scriptPubKey, {},false);
}

bool BlockAssembler::isStillDependent(CTxMemPool::txiter iter)
{
    BOOST_FOREACH(CTxMemPool::txiter parent, mempool.GetMemPoolParents(iter))
    {
        if (!inBlock.count(parent)) {
            return true;
        }
    }
    return false;
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost)
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
// - serialized size (in case -blockmaxsize is in use)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    uint64_t nPotentialBlockSize = nBlockSize; // only used with fNeedSizeAccounting
    BOOST_FOREACH (const CTxMemPool::txiter it, package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && !it->GetTx().wit.IsNull())
            return false;
        if (fNeedSizeAccounting) {
            uint64_t nTxSize = ::GetSerializeSize(it->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
            if (nPotentialBlockSize + nTxSize >= nBlockMaxSize) {
                return false;
            }
            nPotentialBlockSize += nTxSize;
        }
    }
    return true;
}

bool BlockAssembler::TestForBlock(CTxMemPool::txiter iter)
{
    LogPrintf("\nTestForBlock ######################################\n");
    LogPrintf("nBlockMaxSize = %d\n", nBlockMaxSize);
    LogPrintf("nBlockSize = %d\n", nBlockSize);
    int nTxSize = ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    LogPrintf("nTxSize = %d\n", nTxSize);
    LogPrintf("nBlockSize + nTxSize  = %d\n", nBlockSize + nTxSize);
    LogPrintf("######################################\n\n\n");
    LogPrintf("nBlockWeight = %d\n", nBlockWeight);
    LogPrintf("iter->GetTxWeight() = %d\n", iter->GetTxWeight());
    LogPrintf("nBlockWeight = %d\n", nBlockWeight);
    LogPrintf("lastFewTxs = %d\n", lastFewTxs);
    LogPrintf("######################################\n\n\n");

    if (nBlockWeight + iter->GetTxWeight() >= nBlockMaxWeight) {
        // If the block is so close to full that no more txs will fit
        // or if we've tried more than 50 times to fill remaining space
        // then flag that the block is finished
        if (nBlockWeight >  nBlockMaxWeight - 400 || lastFewTxs > 50) {
             blockFinished = true;
             LogPrintf("\nTestForBlock -> FAIL: blockFinished = true\n");
             return false;
        }
        // Once we're within 4000 weight of a full block, only look at 50 more txs
        // to try to fill the remaining space.
        if (nBlockWeight > nBlockMaxWeight - 4000) {
            lastFewTxs++;
        }
        LogPrintf("\nTestForBlock -> FAIL: nBlockWeight + iter->GetTxWeight() >= nBlockMaxWeight\n");
        return false;
    }

    if (fNeedSizeAccounting) {
        if (nBlockSize + ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION) >= nBlockMaxSize) {
            if (nBlockSize >  nBlockMaxSize - 100 || lastFewTxs > 50) {
                 blockFinished = true;
                 LogPrintf("\nTestForBlock -> FAIL: fNeedSizeAccounting: blockFinished = true\n");
                 return false;
            }
            if (nBlockSize > nBlockMaxSize - 1000) {
                lastFewTxs++;
            }
            LogPrintf("\nTestForBlock -> FAIL: fNeedSizeAccounting\n");
            return false;
        }
    }

    if (nBlockSigOpsCost + iter->GetSigOpCost() >= MAX_BLOCK_SIGOPS_COST) {
        // If the block has room for no more sig ops then
        // flag that the block is finished
        if (nBlockSigOpsCost > MAX_BLOCK_SIGOPS_COST - 8) {
            blockFinished = true;
            LogPrintf("\nTestForBlock -> FAIL: nBlockSigOpsCost: blockFinished = true\n");
            return false;
        }
        // Otherwise attempt to find another tx with fewer sigops
        // to put in the block.
        LogPrintf("\nTestForBlock -> FAIL: nBlockSigOpsCost\n");
        return false;
    }

    // Must check that lock times are still valid
    // This can be removed once MTP is always enforced
    // as long as reorgs keep the mempool consistent.
    if (!IsFinalTx(iter->GetTx(), nHeight, nLockTimeCutoff)) {
        LogPrintf("\nTestForBlock -> FAIL: !IsFinalTx()\n");
        return false;
    }

    LogPrintf("\nTestForBlock -> OK\n");
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.push_back(iter->GetTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    if (fNeedSizeAccounting) {
        nBlockSize += ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    }
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        double dPriority = iter->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(iter->GetTx().GetHash(), dPriority, dummy);
        LogPrintf("priority %.1f fee %s txid %s\n",
                  dPriority,
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

void BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    BOOST_FOREACH(const CTxMemPool::txiter it, alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        BOOST_FOREACH(CTxMemPool::txiter desc, descendants) {
            if (alreadyAdded.count(desc))
                continue;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it))
        return true;
    return false;
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs()
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;
    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareModifiedEntry()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < ::minRelayTxFee.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // Package can be added. Sort the entries in a valid order.
        vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        // Update transactions that depend on each of these
        UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void BlockAssembler::addPriorityTxs()
{
    // Largest block you're willing to create:
    nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SERIALIZED_SIZE - 1000), nBlockMaxSize));
    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

//    if (nBlockPrioritySize == 0) {
//        return;
//    }

    bool fSizeAccounting = fNeedSizeAccounting;
    fNeedSizeAccounting = true;

    // This vector will be sorted into a priority queue:
    vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash> waitPriMap;
    typedef std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    vecPriority.reserve(mempool.mapTx.size());
    for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
         mi != mempool.mapTx.end(); ++mi)
    {
        double dPriority = mi->GetPriority(nHeight);
        CAmount dummy;
        CTransaction tx = mi->GetTx();
        mempool.ApplyDeltas(tx.GetHash(), dPriority, dummy);
        vecPriority.push_back(TxCoinAgePriority(dPriority, mi));
        //add Index validation
        if (tx.IsCoinBase() || !CheckFinalTx(tx))
            continue;
        if (tx.IsZerocoinSpend() || tx.IsSigmaSpend() || tx.IsZerocoinRemint()) {
            //mempool.countZCSpend--;
            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

            LogPrintf("\n\n######################################\n");
            LogPrintf("nBlockMaxSize = %d\n", nBlockMaxSize);
            LogPrintf("nBlockSize = %d\n", nBlockSize);
            LogPrintf("nTxSize = %d\n", nTxSize);
            LogPrintf("nBlockSize + nTxSize  = %d\n", nBlockSize + nTxSize);
            LogPrintf("######################################\n\n\n");

            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOpsCost + nTxSigOps >= MAX_BLOCK_SIGOPS_COST)
                continue;

            CAmount nTxFees(0);
            if (tx.IsSigmaSpend())
                nTxFees = mi->GetFee();

            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOpsCost.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOpsCost += nTxSigOps;
            nFees += nTxFees;
            continue;
        }
    }
    std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);

    CTxMemPool::txiter iter;
    while (!vecPriority.empty() && !blockFinished) { // add a tx from priority queue to fill the blockprioritysize
        iter = vecPriority.front().second;
        actualPriority = vecPriority.front().first;
        std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        vecPriority.pop_back();

        // If tx already in block, skip
        if (inBlock.count(iter)) {
            assert(false); // shouldn't happen for priority txs
            continue;
        }

        // cannot accept witness transactions into a non-witness block
        if (!fIncludeWitness && !iter->GetTx().wit.IsNull())
            continue;

        // If tx is dependent on other mempool txs which haven't yet been included
        // then put it in the waitSet
        if (isStillDependent(iter)) {
            waitPriMap.insert(std::make_pair(iter, actualPriority));
            continue;
        }

        // If this tx fits in the block add it, otherwise keep looping
        if (TestForBlock(iter)) {
            AddToBlock(iter);

            // If now that this txs is added we've surpassed our desired priority size
            // or have dropped below the AllowFreeThreshold, then we're done adding priority txs
            if (nBlockSize >= nBlockPrioritySize || !AllowFree(actualPriority)) {
                break;
            }

            // This tx was successfully added, so
            // add transactions that depend on this one to the priority queue to try again
            BOOST_FOREACH(CTxMemPool::txiter child, mempool.GetMemPoolChildren(iter))
            {
                waitPriIter wpiter = waitPriMap.find(child);
                if (wpiter != waitPriMap.end()) {
                    vecPriority.push_back(TxCoinAgePriority(wpiter->second,child));
                    std::push_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                    waitPriMap.erase(wpiter);
                }
            }
        }
    }
    fNeedSizeAccounting = fSizeAccounting;
}

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// The nonce is usually preserved between calls, but periodically or if the
// nonce is 0xffff0000 or above, the block is rebuilt and nNonce starts over at
// zero.
//
//bool static ScanHash(const CBlockHeader *pblock, uint32_t& nNonce, uint256 *phash)
//{
//    // Write the first 76 bytes of the block header to a double-SHA256 state.
//    CHash256 hasher;
//    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
//    ss << *pblock;
//    assert(ss.size() == 80);
//    hasher.Write((unsigned char*)&ss[0], 76);
//
//    while (true) {
//        nNonce++;
//
//        // Write the last 4 bytes of the block header (the nonce) to a copy of
//        // the double-SHA256 state, and compute the result.
//        CHash256(hasher).Write((unsigned char*)&nNonce, 4).Finalize((unsigned char*)phash);
//
//        // Return the nonce if the hash has at least some zero bits,
//        // caller will check if it has enough to reach the target
//        if (((uint16_t*)phash)[15] == 0)
//            return true;
//
//        // If nothing found after trying for a while, return -1
//        if ((nNonce & 0xfff) == 0)
//            return false;
//    }
//}

static bool ProcessBlockFound(const CBlock* pblock, const CChainParams& chainparams)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BitcoinMiner: generated block is stale");
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, chainparams, NULL, pblock, true, NULL, false))
        return error("ZcoinMiner: ProcessNewBlock, block not accepted");

    return true;
}

void static ZcoinMiner(const CChainParams &chainparams) {
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("fiveg-miner");

    unsigned int nExtraNonce = 0;

    boost::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);
    bool fTestNet = chainparams.GetConsensus().IsTestnet();
    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbaseScript || coinbaseScript->reserveScript.empty()) {
            LogPrintf("ZcoinMiner stop here coinbaseScript=%s, coinbaseScript->reserveScript.empty()=%s\n", coinbaseScript, coinbaseScript->reserveScript.empty());
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");
        }

        while (true) {
            if (chainparams.MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.

                // Also try to wait for fivegnode winners unless we're on regtest chain
                do {
                    bool fvNodesEmpty;
                    bool fHasFivegnodesWinnerForNextBlock;
                    const Consensus::Params &params = chainparams.GetConsensus();
                    {
                        LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    {
                        LOCK2(cs_main, mempool.cs);
                        int nCount = 0;
                        fHasFivegnodesWinnerForNextBlock =
                                params.IsRegtest() ||
                                chainActive.Height() < params.nFivegnodePaymentsStartBlock ||
                                mnodeman.GetNextFivegnodeInQueueForPayment(chainActive.Height(), true, nCount);
                    }
                    if (!fvNodesEmpty && fHasFivegnodesWinnerForNextBlock && !IsInitialBlockDownload()) {
                        break;
                    }
                    MilliSleep(1000);
                } while (true);
            }
            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex *pindexPrev = chainActive.Tip();
            if (pindexPrev) {
                LogPrintf("loop pindexPrev->nHeight=%s\n", pindexPrev->nHeight);
            }
            unique_ptr <CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(
                coinbaseScript->reserveScript, {}));
            if (!pblocktemplate.get()) {
                LogPrintf("Error in ZcoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("Running ZcoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                      ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            //
            // Search
            //
            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            LogPrintf("hashTarget: %s\n", hashTarget.ToString());
            LogPrintf("fTestnet: %d\n", fTestNet);
            LogPrintf("pindexPrev->nHeight: %s\n", pindexPrev->nHeight);
            LogPrintf("pblock: %s\n", pblock->ToString());
            LogPrintf("pblock->nVersion: %s\n", pblock->nVersion);
            LogPrintf("pblock->nTime: %s\n", pblock->nTime);
            LogPrintf("pblock->nNonce: %s\n", &pblock->nNonce);
            LogPrintf("powLimit: %s\n", Params().GetConsensus().powLimit.ToString());

            while (true) {
                // Check if something found
                uint256 thash;
                   ///change to x116rv3
                while (true) {
                    thash = pblock->GetPoWHash();

                    //LogPrintf("*****\nhash   : %s  \ntarget : %s\n", UintToArith256(thash).ToString(), hashTarget.ToString());

                    if (UintToArith256(thash) <= hashTarget) {
                        // Found a solution
                        LogPrintf("Found a solution. Hash: %s", UintToArith256(thash).ToString());
                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
//                        CheckWork(pblock, *pwallet, reservekey);
                        LogPrintf("ZcoinMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", UintToArith256(thash).ToString(), hashTarget.ToString());

                        if(!ProcessBlockFound(pblock, chainparams)){
                            break;
                            throw boost::thread_interrupted();
                        }
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);
                        coinbaseScript->KeepScript();
                        // In regression test mode, stop mining after a block is found.
                        if (chainparams.MineBlocksOnDemand())
                            throw boost::thread_interrupted();
                        break;
                    }
                    pblock->nNonce += 1;
                    if ((pblock->nNonce & 0xFF) == 0)
                        break;
                }
                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if (vNodes.empty() && chainparams.MiningRequiresPeers())
                    break;
                if (pblock->nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                if (UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev) < 0)
                    break; // Recreate the block if the clock has run backwards,
                // so that we can use the correct time.
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks) {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted &) {
        LogPrintf("ZcoinMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e) {
        LogPrintf("ZcoinMiner runtime error: %s\n", e.what());
        return;
    }
}

void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&ZcoinMiner, boost::cref(chainparams)));
}

void ThreadStakeMiner(CWallet *pwallet, const CChainParams& chainparams)
{
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    LogPrintf("Staking started\n");

    // Make this thread recognisable as the mining thread
    RenameThread("fiveg-staker");

    CReserveKey reservekey(pwallet);

    bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);
    bool fTryToSync = true;
    while (true)
    {
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;            
        if (nHeight >= Params().GetConsensus().nFirstPOSBlock)
        {
            while (pwallet->IsLocked())
            {
                nLastCoinStakeSearchInterval = 0;
                MilliSleep(10000);
            }
            while (vNodes.empty() || IsInitialBlockDownload() || !fivegnodeSync.IsSynced())
            {
                nLastCoinStakeSearchInterval = 0;
                fTryToSync = true;
                MilliSleep(1000);
            }
            if (fTryToSync)
            {
                fTryToSync = false;
                if (vNodes.size() < 2 || pindexBestHeader->GetBlockTime() < GetTime() - 10 * 60)
                {
                    MilliSleep(6000);
                    continue;
                }
            }

            //
            // Create new block
            //
            if (pwallet->HaveAvailableCoinsForStaking()) {
                int64_t nFees = 0;
                // First just create an empty block. No need to process transactions until we know we can create a block
                std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(reservekey.reserveScript, {},true));
                if (!pblocktemplate.get()) {
                    LogPrintf("ThreadStakeMiner(): Could not get Blocktemplate\n");
                    return;
                }

                CBlock *pblock = &pblocktemplate->block;
                // Trying to sign a block
                if (SignBlock(*pblock, *pwallet, nFees, pblocktemplate.get()))
                {
                    // increase priority
                    SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL);
                     // Check if stake check passes and process the new block
                    CheckStake(pblock, *pwallet, chainparams);
                    // return back to low priority
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);
                    MilliSleep(5000);
                }
            }
            MilliSleep(nMinerSleep);
        }
        MilliSleep(10000);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}
