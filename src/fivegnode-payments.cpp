// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activefivegnode.h"
#include "darksend.h"
#include "fivegnode-payments.h"
#include "fivegnode-sync.h"
#include "fivegnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CFivegnodePayments mnpayments;

CCriticalSection cs_vecPayees;
CCriticalSection cs_mapFivegnodeBlocks;
CCriticalSection cs_mapFivegnodePaymentVotes;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In Index some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock &block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet) {
    strErrorRet = "";

    bool isBlockRewardValueMet = (block.vtx[0].GetValueOut() <= blockReward);
    if (fDebug) LogPrintf("block.vtx[0].GetValueOut() %lld <= blockReward %lld\n", block.vtx[0].GetValueOut(), blockReward);

    // we are still using budgets, but we have no data about them anymore,
    // all we know is predefined budget cycle and window

//    const Consensus::Params &consensusParams = Params().GetConsensus();
//
////    if (nBlockHeight < consensusParams.nSuperblockStartBlock) {
//        int nOffset = nBlockHeight % consensusParams.nBudgetPaymentsCycleBlocks;
//        if (nBlockHeight >= consensusParams.nBudgetPaymentsStartBlock &&
//            nOffset < consensusParams.nBudgetPaymentsWindowBlocks) {
//            // NOTE: make sure SPORK_13_OLD_SUPERBLOCK_FLAG is disabled when 12.1 starts to go live
//            if (fivegnodeSync.IsSynced() && !sporkManager.IsSporkActive(SPORK_13_OLD_SUPERBLOCK_FLAG)) {
//                // no budget blocks should be accepted here, if SPORK_13_OLD_SUPERBLOCK_FLAG is disabled
//                LogPrint("gobject", "IsBlockValueValid -- Client synced but budget spork is disabled, checking block value against block reward\n");
//                if (!isBlockRewardValueMet) {
//                    strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, budgets are disabled",
//                                            nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//                }
//                return isBlockRewardValueMet;
//            }
//            LogPrint("gobject", "IsBlockValueValid -- WARNING: Skipping budget block value checks, accepting block\n");
//            // TODO: reprocess blocks to make sure they are legit?
//            return true;
//        }
//        // LogPrint("gobject", "IsBlockValueValid -- Block is not in budget cycle window, checking block value against block reward\n");
//        if (!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, block is not in budget cycle window",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//        return isBlockRewardValueMet;
//    }

    // superblocks started

//    CAmount nSuperblockMaxValue =  blockReward + CSuperblock::GetPaymentsLimit(nBlockHeight);
//    bool isSuperblockMaxValueMet = (block.vtx[0].GetValueOut() <= nSuperblockMaxValue);
//    bool isSuperblockMaxValueMet = false;

//    LogPrint("gobject", "block.vtx[0].GetValueOut() %lld <= nSuperblockMaxValue %lld\n", block.vtx[0].GetValueOut(), nSuperblockMaxValue);

    if (!fivegnodeSync.IsSynced()) {
        // not enough data but at least it must NOT exceed superblock max value
//        if(CSuperblock::IsValidBlockHeight(nBlockHeight)) {
//            if(fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, checking superblock max bounds only\n");
//            if(!isSuperblockMaxValueMet) {
//                strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded superblock max value",
//                                        nBlockHeight, block.vtx[0].GetValueOut(), nSuperblockMaxValue);
//            }
//            return isSuperblockMaxValueMet;
//        }
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
        // it MUST be a regular block otherwise
        return isBlockRewardValueMet;
    }

    // we are synced, let's try to check as much data as we can

    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
////        if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
////            if(CSuperblockManager::IsValid(block.vtx[0], nBlockHeight, blockReward)) {
////                LogPrint("gobject", "IsBlockValueValid -- Valid superblock at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////                // all checks are done in CSuperblock::IsValid, nothing to do here
////                return true;
////            }
////
////            // triggered but invalid? that's weird
////            LogPrintf("IsBlockValueValid -- ERROR: Invalid superblock detected at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////            // should NOT allow invalid superblocks, when superblocks are enabled
////            strErrorRet = strprintf("invalid superblock detected at height %d", nBlockHeight);
////            return false;
////        }
//        LogPrint("gobject", "IsBlockValueValid -- No triggered superblock detected at height %d\n", nBlockHeight);
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, no triggered superblock detected",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
        LogPrint("gobject", "IsBlockValueValid -- Superblocks are disabled, no superblocks allowed\n");
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
    }

    // it MUST be a regular block
    return isBlockRewardValueMet;
}

bool IsBlockPayeeValid(const CTransaction &txNew, int nBlockHeight, CAmount blockReward) {
    // we can only check fivegnode payment /
    const Consensus::Params &consensusParams = Params().GetConsensus();

    if (nBlockHeight < consensusParams.nFivegnodePaymentsStartBlock) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- fivegnode isn't start\n");
        return true;
    }
    if (!fivegnodeSync.IsSynced() && Params().NetworkIDString() != CBaseChainParams::REGTEST) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, skipping block payee checks\n");
        return true;
    }

    //check for fivegnode payee
    if (mnpayments.IsTransactionValid(txNew, nBlockHeight, false)) {
        LogPrint("mnpayments", "IsBlockPayeeValid -- Valid fivegnode payment at height %d: %s", nBlockHeight, txNew.ToString());
        return true;
    } else {
        if(sporkManager.IsSporkActive(SPORK_8_FIVEGNODE_PAYMENT_ENFORCEMENT)){
            return false;
        } else {
            LogPrintf("FivegNode payment enforcement is disabled, accepting block\n");
            return true;
        }
    }
}

void FillBlockPayments(CMutableTransaction &txNew, int nBlockHeight, CAmount fivegnodePayment, CTxOut &txoutFivegnodeRet, std::vector <CTxOut> &voutSuperblockRet) {
    // only create superblocks if spork is enabled AND if superblock is actually triggered
    // (height should be validated inside)
//    if(sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED) &&
//        CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//            LogPrint("gobject", "FillBlockPayments -- triggered superblock creation at height %d\n", nBlockHeight);
//            CSuperblockManager::CreateSuperblock(txNew, nBlockHeight, voutSuperblockRet);
//            return;
//    }

    // FILL BLOCK PAYEE WITH FIVEGNODE PAYMENT OTHERWISE
    mnpayments.FillBlockPayee(txNew, nBlockHeight, fivegnodePayment, txoutFivegnodeRet);
    LogPrint("mnpayments", "FillBlockPayments -- nBlockHeight %d fivegnodePayment %lld txoutFivegnodeRet %s txNew %s",
             nBlockHeight, fivegnodePayment, txoutFivegnodeRet.ToString(), txNew.ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight) {
    // IF WE HAVE A ACTIVATED TRIGGER FOR THIS HEIGHT - IT IS A SUPERBLOCK, GET THE REQUIRED PAYEES
//    if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//        return CSuperblockManager::GetRequiredPaymentsString(nBlockHeight);
//    }

    // OTHERWISE, PAY FIVEGNODE
    return mnpayments.GetRequiredPaymentsString(nBlockHeight);
}

void CFivegnodePayments::Clear() {
    LOCK2(cs_mapFivegnodeBlocks, cs_mapFivegnodePaymentVotes);
    mapFivegnodeBlocks.clear();
    mapFivegnodePaymentVotes.clear();
}

bool CFivegnodePayments::CanVote(COutPoint outFivegnode, int nBlockHeight) {
    LOCK(cs_mapFivegnodePaymentVotes);

    if (mapFivegnodesLastVote.count(outFivegnode) && mapFivegnodesLastVote[outFivegnode] == nBlockHeight) {
        return false;
    }

    //record this fivegnode voted
    mapFivegnodesLastVote[outFivegnode] = nBlockHeight;
    return true;
}

std::string CFivegnodePayee::ToString() const {
    CTxDestination address1;
    ExtractDestination(scriptPubKey, address1);
    CBitcoinAddress address2(address1);
    std::string str;
    str += "(address: ";
    str += address2.ToString();
    str += ")\n";
    return str;
}

/**
*   FillBlockPayee
*
*   Fill Fivegnode ONLY payment block
*/

void CFivegnodePayments::FillBlockPayee(CMutableTransaction &txNew, int nBlockHeight, CAmount fivegnodePayment, CTxOut &txoutFivegnodeRet) {
    // make sure it's not filled yet
    txoutFivegnodeRet = CTxOut();

    CScript payee;
    bool foundMaxVotedPayee = true;

    if (!mnpayments.GetBlockPayee(nBlockHeight, payee)) {
        // no fivegnode detected...
        // LogPrintf("no fivegnode detected...\n");
        foundMaxVotedPayee = false;
        int nCount = 0;
        CFivegnode *winningNode = mnodeman.GetNextFivegnodeInQueueForPayment(nBlockHeight, true, nCount);
        if (!winningNode) {
            if(Params().NetworkIDString() != CBaseChainParams::REGTEST) {
                // ...and we can't calculate it on our own
                LogPrintf("CFivegnodePayments::FillBlockPayee -- Failed to detect fivegnode to pay\n");
                return;
            }
        }
        // fill payee with locally calculated winner and hope for the best
        if (winningNode) {
            payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
            LogPrintf("payee=%s\n", winningNode->ToString());
        }
        else
            payee = txNew.vout[0].scriptPubKey;//This is only for unit tests scenario on REGTEST
    }
    txoutFivegnodeRet = CTxOut(fivegnodePayment, payee);
    txNew.vout.push_back(txoutFivegnodeRet);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);
    if (foundMaxVotedPayee) {
        LogPrintf("CFivegnodePayments::FillBlockPayee::foundMaxVotedPayee -- Fivegnode payment %lld to %s\n", fivegnodePayment, address2.ToString());
    } else {
        LogPrintf("CFivegnodePayments::FillBlockPayee -- Fivegnode payment %lld to %s\n", fivegnodePayment, address2.ToString());
    }

}

int CFivegnodePayments::GetMinFivegnodePaymentsProto() {
    return sporkManager.IsSporkActive(SPORK_10_FIVEGNODE_PAY_UPDATED_NODES)
           ? MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_2
           : MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_1;
}

void CFivegnodePayments::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {

//    LogPrintf("CFivegnodePayments::ProcessMessage strCommand=%s\n", strCommand);
    // Ignore any payments messages until fivegnode list is synced
    if (!fivegnodeSync.IsFivegnodeListSynced()) return;

    if (fLiteMode) return; // disable all Index specific functionality

    bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);

    if (strCommand == NetMsgType::FIVEGNODEPAYMENTSYNC) { //Fivegnode Payments Request Sync

        // Ignore such requests until we are fully synced.
        // We could start processing this after fivegnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!fivegnodeSync.IsSynced()) return;

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::FIVEGNODEPAYMENTSYNC)) {
            // Asking for the payments list multiple times in a short period of time is no good
            LogPrintf("FIVEGNODEPAYMENTSYNC -- peer already asked me for the list, peer=%d\n", pfrom->id);
            if (!fTestNet) Misbehaving(pfrom->GetId(), 20);
            return;
        }
        netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::FIVEGNODEPAYMENTSYNC);

        Sync(pfrom);
        LogPrint("mnpayments", "FIVEGNODEPAYMENTSYNC -- Sent Fivegnode payment votes to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::FIVEGNODEPAYMENTVOTE) { // Fivegnode Payments Vote for the Winner

        CFivegnodePaymentVote vote;
        vRecv >> vote;

        if (pfrom->nVersion < GetMinFivegnodePaymentsProto()) return;

        if (!pCurrentBlockIndex) return;

        uint256 nHash = vote.GetHash();

        pfrom->setAskFor.erase(nHash);

        {
            LOCK(cs_mapFivegnodePaymentVotes);
            if (mapFivegnodePaymentVotes.count(nHash)) {
                LogPrint("mnpayments", "FIVEGNODEPAYMENTVOTE -- hash=%s, nHeight=%d seen\n", nHash.ToString(), pCurrentBlockIndex->nHeight);
                return;
            }

            // Avoid processing same vote multiple times
            mapFivegnodePaymentVotes[nHash] = vote;
            // but first mark vote as non-verified,
            // AddPaymentVote() below should take care of it if vote is actually ok
            mapFivegnodePaymentVotes[nHash].MarkAsNotVerified();
        }

        int nFirstBlock = pCurrentBlockIndex->nHeight - GetStorageLimit();
        if (vote.nBlockHeight < nFirstBlock || vote.nBlockHeight > pCurrentBlockIndex->nHeight + 20) {
            LogPrint("mnpayments", "FIVEGNODEPAYMENTVOTE -- vote out of range: nFirstBlock=%d, nBlockHeight=%d, nHeight=%d\n", nFirstBlock, vote.nBlockHeight, pCurrentBlockIndex->nHeight);
            return;
        }

        std::string strError = "";
        if (!vote.IsValid(pfrom, pCurrentBlockIndex->nHeight, strError)) {
            LogPrint("mnpayments", "FIVEGNODEPAYMENTVOTE -- invalid message, error: %s\n", strError);
            return;
        }

        if (!CanVote(vote.vinFivegnode.prevout, vote.nBlockHeight)) {
            LogPrintf("FIVEGNODEPAYMENTVOTE -- fivegnode already voted, fivegnode=%s\n", vote.vinFivegnode.prevout.ToStringShort());
            return;
        }

        fivegnode_info_t mnInfo = mnodeman.GetFivegnodeInfo(vote.vinFivegnode);
        if (!mnInfo.fInfoValid) {
            // mn was not found, so we can't check vote, some info is probably missing
            LogPrintf("FIVEGNODEPAYMENTVOTE -- fivegnode is missing %s\n", vote.vinFivegnode.prevout.ToStringShort());
            mnodeman.AskForMN(pfrom, vote.vinFivegnode);
            return;
        }

        int nDos = 0;
        if (!vote.CheckSignature(mnInfo.pubKeyFivegnode, pCurrentBlockIndex->nHeight, nDos)) {
            if (nDos) {
                LogPrintf("FIVEGNODEPAYMENTVOTE -- ERROR: invalid signature\n");
                if (!fTestNet) Misbehaving(pfrom->GetId(), nDos);
            } else {
                // only warn about anything non-critical (i.e. nDos == 0) in debug mode
                LogPrint("mnpayments", "FIVEGNODEPAYMENTVOTE -- WARNING: invalid signature\n");
            }
            // Either our info or vote info could be outdated.
            // In case our info is outdated, ask for an update,
            mnodeman.AskForMN(pfrom, vote.vinFivegnode);
            // but there is nothing we can do if vote info itself is outdated
            // (i.e. it was signed by a mn which changed its key),
            // so just quit here.
            return;
        }

        CTxDestination address1;
        ExtractDestination(vote.payee, address1);
        CBitcoinAddress address2(address1);

        LogPrint("mnpayments", "FIVEGNODEPAYMENTVOTE -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), vote.nBlockHeight, pCurrentBlockIndex->nHeight, vote.vinFivegnode.prevout.ToStringShort());

        if (AddPaymentVote(vote)) {
            vote.Relay();
            fivegnodeSync.AddedPaymentVote();
        }
    }
}

bool CFivegnodePaymentVote::Sign() {
    std::string strError;
    std::string strMessage = vinFivegnode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, activeFivegnode.keyFivegnode)) {
        LogPrintf("CFivegnodePaymentVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(activeFivegnode.pubKeyFivegnode, vchSig, strMessage, strError)) {
        LogPrintf("CFivegnodePaymentVote::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CFivegnodePayments::GetBlockPayee(int nBlockHeight, CScript &payee) {
    if (mapFivegnodeBlocks.count(nBlockHeight)) {
        return mapFivegnodeBlocks[nBlockHeight].GetBestPayee(payee);
    }

    return false;
}

// Is this fivegnode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CFivegnodePayments::IsScheduled(CFivegnode &mn, int nNotBlockHeight) {
    LOCK(cs_mapFivegnodeBlocks);

    if (!pCurrentBlockIndex) return false;

    CScript mnpayee;
    mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = pCurrentBlockIndex->nHeight; h <= pCurrentBlockIndex->nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapFivegnodeBlocks.count(h) && mapFivegnodeBlocks[h].GetBestPayee(payee) && mnpayee == payee) {
            return true;
        }
    }

    return false;
}

bool CFivegnodePayments::AddPaymentVote(const CFivegnodePaymentVote &vote) {
    LogPrint("fivegnode-payments", "CFivegnodePayments::AddPaymentVote\n");
    uint256 blockHash = uint256();
    if (!GetBlockHash(blockHash, vote.nBlockHeight - 101)) return false;

    if (HasVerifiedPaymentVote(vote.GetHash())) return false;

    LOCK2(cs_mapFivegnodeBlocks, cs_mapFivegnodePaymentVotes);

    mapFivegnodePaymentVotes[vote.GetHash()] = vote;

    if (!mapFivegnodeBlocks.count(vote.nBlockHeight)) {
        CFivegnodeBlockPayees blockPayees(vote.nBlockHeight);
        mapFivegnodeBlocks[vote.nBlockHeight] = blockPayees;
    }

    mapFivegnodeBlocks[vote.nBlockHeight].AddPayee(vote);

    return true;
}

bool CFivegnodePayments::HasVerifiedPaymentVote(uint256 hashIn) {
    LOCK(cs_mapFivegnodePaymentVotes);
    std::map<uint256, CFivegnodePaymentVote>::iterator it = mapFivegnodePaymentVotes.find(hashIn);
    return it != mapFivegnodePaymentVotes.end() && it->second.IsVerified();
}

void CFivegnodeBlockPayees::AddPayee(const CFivegnodePaymentVote &vote) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CFivegnodePayee & payee, vecPayees)
    {
        if (payee.GetPayee() == vote.payee) {
            payee.AddVoteHash(vote.GetHash());
            return;
        }
    }
    CFivegnodePayee payeeNew(vote.payee, vote.GetHash());
    vecPayees.push_back(payeeNew);
}

bool CFivegnodeBlockPayees::GetBestPayee(CScript &payeeRet) {
    LOCK(cs_vecPayees);
    LogPrint("mnpayments", "CFivegnodeBlockPayees::GetBestPayee, vecPayees.size()=%s\n", vecPayees.size());
    if (!vecPayees.size()) {
        LogPrint("mnpayments", "CFivegnodeBlockPayees::GetBestPayee -- ERROR: couldn't find any payee\n");
        return false;
    }

    int nVotes = -1;
    BOOST_FOREACH(CFivegnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() > nVotes) {
            payeeRet = payee.GetPayee();
            nVotes = payee.GetVoteCount();
        }
    }

    return (nVotes > -1);
}

bool CFivegnodeBlockPayees::HasPayeeWithVotes(CScript payeeIn, int nVotesReq) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CFivegnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nVotesReq && payee.GetPayee() == payeeIn) {
            return true;
        }
    }

//    LogPrint("mnpayments", "CFivegnodeBlockPayees::HasPayeeWithVotes -- ERROR: couldn't find any payee with %d+ votes\n", nVotesReq);
    return false;
}

bool CFivegnodeBlockPayees::IsTransactionValid(const CTransaction &txNew, bool fMTP,int nHeight) {
    LOCK(cs_vecPayees);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";


    CAmount nFivegnodePayment = GetFivegnodePayment(Params().GetConsensus(), fMTP,nHeight);

    //require at least MNPAYMENTS_SIGNATURES_REQUIRED signatures

    BOOST_FOREACH(CFivegnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nMaxSignatures) {
            nMaxSignatures = payee.GetVoteCount();
        }
    }
    LogPrintf("nmaxsig = %s \n",nMaxSignatures);
    // if we don't have at least MNPAYMENTS_SIGNATURES_REQUIRED signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    bool hasValidPayee = false;

    BOOST_FOREACH(CFivegnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            hasValidPayee = true;

            BOOST_FOREACH(CTxOut txout, txNew.vout) {
                if (payee.GetPayee() == txout.scriptPubKey && nFivegnodePayment == txout.nValue) {
                    LogPrint("mnpayments", "CFivegnodeBlockPayees::IsTransactionValid -- Found required payment\n");
                    return true;
                }
            }

            CTxDestination address1;
            ExtractDestination(payee.GetPayee(), address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible = address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    LogPrintf("CFivegnodeBlockPayees::IsTransactionValid -- ERROR: Missing required payment, possible payees: '%s', amount: %f VGC\n", strPayeesPossible, (float) nFivegnodePayment / COIN);
    return false;
}

std::string CFivegnodeBlockPayees::GetRequiredPaymentsString() {
    LOCK(cs_vecPayees);

    std::string strRequiredPayments = "Unknown";

    BOOST_FOREACH(CFivegnodePayee & payee, vecPayees)
    {
        CTxDestination address1;
        ExtractDestination(payee.GetPayee(), address1);
        CBitcoinAddress address2(address1);

        if (strRequiredPayments != "Unknown") {
            strRequiredPayments += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        } else {
            strRequiredPayments = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        }
    }

    return strRequiredPayments;
}

std::string CFivegnodePayments::GetRequiredPaymentsString(int nBlockHeight) {
    LOCK(cs_mapFivegnodeBlocks);

    if (mapFivegnodeBlocks.count(nBlockHeight)) {
        return mapFivegnodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CFivegnodePayments::IsTransactionValid(const CTransaction &txNew, int nBlockHeight, bool fMTP) {
    LOCK(cs_mapFivegnodeBlocks);

    if (mapFivegnodeBlocks.count(nBlockHeight)) {
        return mapFivegnodeBlocks[nBlockHeight].IsTransactionValid(txNew, fMTP,nBlockHeight);
    }

    return true;
}

void CFivegnodePayments::CheckAndRemove() {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_mapFivegnodeBlocks, cs_mapFivegnodePaymentVotes);

    int nLimit = GetStorageLimit();

    std::map<uint256, CFivegnodePaymentVote>::iterator it = mapFivegnodePaymentVotes.begin();
    while (it != mapFivegnodePaymentVotes.end()) {
        CFivegnodePaymentVote vote = (*it).second;

        if (pCurrentBlockIndex->nHeight - vote.nBlockHeight > nLimit) {
            LogPrint("mnpayments", "CFivegnodePayments::CheckAndRemove -- Removing old Fivegnode payment: nBlockHeight=%d\n", vote.nBlockHeight);
            mapFivegnodePaymentVotes.erase(it++);
            mapFivegnodeBlocks.erase(vote.nBlockHeight);
        } else {
            ++it;
        }
    }
    LogPrintf("CFivegnodePayments::CheckAndRemove -- %s\n", ToString());
}

bool CFivegnodePaymentVote::IsValid(CNode *pnode, int nValidationHeight, std::string &strError) {
    CFivegnode *pmn = mnodeman.Find(vinFivegnode);

    if (!pmn) {
        strError = strprintf("Unknown Fivegnode: prevout=%s", vinFivegnode.prevout.ToStringShort());
        // Only ask if we are already synced and still have no idea about that Fivegnode
        if (fivegnodeSync.IsFivegnodeListSynced()) {
            mnodeman.AskForMN(pnode, vinFivegnode);
        }

        return false;
    }

    int nMinRequiredProtocol;
    if (nBlockHeight >= nValidationHeight) {
        // new votes must comply SPORK_10_FIVEGNODE_PAY_UPDATED_NODES rules
        nMinRequiredProtocol = mnpayments.GetMinFivegnodePaymentsProto();
    } else {
        // allow non-updated fivegnodes for old blocks
        nMinRequiredProtocol = MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_1;
    }

    if (pmn->nProtocolVersion < nMinRequiredProtocol) {
        strError = strprintf("Fivegnode protocol is too old: nProtocolVersion=%d, nMinRequiredProtocol=%d", pmn->nProtocolVersion, nMinRequiredProtocol);
        return false;
    }

    // Only fivegnodes should try to check fivegnode rank for old votes - they need to pick the right winner for future blocks.
    // Regular clients (miners included) need to verify fivegnode rank for future block votes only.
    if (!fFivegNode && nBlockHeight < nValidationHeight) return true;

    int nRank = mnodeman.GetFivegnodeRank(vinFivegnode, nBlockHeight - 101, nMinRequiredProtocol, false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CFivegnodePaymentVote::IsValid -- Can't calculate rank for fivegnode %s\n",
                 vinFivegnode.prevout.ToStringShort());
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        // It's common to have fivegnodes mistakenly think they are in the top 10
        // We don't want to print all of these messages in normal mode, debug mode should print though
        strError = strprintf("Fivegnode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        // Only ban for new mnw which is out of bounds, for old mnw MN list itself might be way too much off
        if (nRank > MNPAYMENTS_SIGNATURES_TOTAL * 2 && nBlockHeight > nValidationHeight) {
            strError = strprintf("Fivegnode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, nRank);
            LogPrintf("CFivegnodePaymentVote::IsValid -- Error: %s\n", strError);
            Misbehaving(pnode->GetId(), 20);
        }
        // Still invalid however
        return false;
    }

    return true;
}

bool CFivegnodePayments::ProcessBlock(int nBlockHeight) {

    // DETERMINE IF WE SHOULD BE VOTING FOR THE NEXT PAYEE

    if (fLiteMode || !fFivegNode) {
        return false;
    }

    // We have little chances to pick the right winner if winners list is out of sync
    // but we have no choice, so we'll try. However it doesn't make sense to even try to do so
    // if we have not enough data about fivegnodes.
    if (!fivegnodeSync.IsFivegnodeListSynced()) {
        return false;
    }

    int nRank = mnodeman.GetFivegnodeRank(activeFivegnode.vin, nBlockHeight - 101, GetMinFivegnodePaymentsProto(), false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CFivegnodePayments::ProcessBlock -- Unknown Fivegnode\n");
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrint("mnpayments", "CFivegnodePayments::ProcessBlock -- Fivegnode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        return false;
    }

    // LOCATE THE NEXT FIVEGNODE WHICH SHOULD BE PAID

    LogPrintf("CFivegnodePayments::ProcessBlock -- Start: nBlockHeight=%d, fivegnode=%s\n", nBlockHeight, activeFivegnode.vin.prevout.ToStringShort());

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    CFivegnode *pmn = mnodeman.GetNextFivegnodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == NULL) {
        LogPrintf("CFivegnodePayments::ProcessBlock -- ERROR: Failed to find fivegnode to pay\n");
        return false;
    }

    LogPrintf("CFivegnodePayments::ProcessBlock -- Fivegnode found by GetNextFivegnodeInQueueForPayment(): %s\n", pmn->vin.prevout.ToStringShort());


    CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());

    CFivegnodePaymentVote voteNew(activeFivegnode.vin, nBlockHeight, payee);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    // SIGN MESSAGE TO NETWORK WITH OUR FIVEGNODE KEYS

    if (voteNew.Sign()) {
        if (AddPaymentVote(voteNew)) {
            voteNew.Relay();
            return true;
        }
    }

    return false;
}

void CFivegnodePaymentVote::Relay() {
    // do not relay until synced
    if (!fivegnodeSync.IsWinnersListSynced()) {
        LogPrint("fivegnode", "CFivegnodePaymentVote::Relay - fivegnodeSync.IsWinnersListSynced() not sync\n");
        return;
    }
    CInv inv(MSG_FIVEGNODE_PAYMENT_VOTE, GetHash());
    RelayInv(inv);
}

bool CFivegnodePaymentVote::CheckSignature(const CPubKey &pubKeyFivegnode, int nValidationHeight, int &nDos) {
    // do not ban by default
    nDos = 0;

    std::string strMessage = vinFivegnode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    std::string strError = "";
    if (!darkSendSigner.VerifyMessage(pubKeyFivegnode, vchSig, strMessage, strError)) {
        // Only ban for future block vote when we are already synced.
        // Otherwise it could be the case when MN which signed this vote is using another key now
        // and we have no idea about the old one.
        if (fivegnodeSync.IsFivegnodeListSynced() && nBlockHeight > nValidationHeight) {
            nDos = 20;
        }
        return error("CFivegnodePaymentVote::CheckSignature -- Got bad Fivegnode payment signature, fivegnode=%s, error: %s", vinFivegnode.prevout.ToStringShort().c_str(), strError);
    }

    return true;
}

std::string CFivegnodePaymentVote::ToString() const {
    std::ostringstream info;

    info << vinFivegnode.prevout.ToStringShort() <<
         ", " << nBlockHeight <<
         ", " << ScriptToAsmStr(payee) <<
         ", " << (int) vchSig.size();

    return info.str();
}

// Send only votes for future blocks, node should request every other missing payment block individually
void CFivegnodePayments::Sync(CNode *pnode) {
    LOCK(cs_mapFivegnodeBlocks);

    if (!pCurrentBlockIndex) return;

    int nInvCount = 0;

    for (int h = pCurrentBlockIndex->nHeight; h < pCurrentBlockIndex->nHeight + 20; h++) {
        if (mapFivegnodeBlocks.count(h)) {
            BOOST_FOREACH(CFivegnodePayee & payee, mapFivegnodeBlocks[h].vecPayees)
            {
                std::vector <uint256> vecVoteHashes = payee.GetVoteHashes();
                BOOST_FOREACH(uint256 & hash, vecVoteHashes)
                {
                    if (!HasVerifiedPaymentVote(hash)) continue;
                    pnode->PushInventory(CInv(MSG_FIVEGNODE_PAYMENT_VOTE, hash));
                    nInvCount++;
                }
            }
        }
    }

    LogPrintf("CFivegnodePayments::Sync -- Sent %d votes to peer %d\n", nInvCount, pnode->id);
    pnode->PushMessage(NetMsgType::SYNCSTATUSCOUNT, FIVEGNODE_SYNC_MNW, nInvCount);
}

// Request low data/unknown payment blocks in batches directly from some node instead of/after preliminary Sync.
void CFivegnodePayments::RequestLowDataPaymentBlocks(CNode *pnode) {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_main, cs_mapFivegnodeBlocks);

    std::vector <CInv> vToFetch;
    int nLimit = GetStorageLimit();

    const CBlockIndex *pindex = pCurrentBlockIndex;

    while (pCurrentBlockIndex->nHeight - pindex->nHeight < nLimit) {
        if (!mapFivegnodeBlocks.count(pindex->nHeight)) {
            // We have no idea about this block height, let's ask
            vToFetch.push_back(CInv(MSG_FIVEGNODE_PAYMENT_BLOCK, pindex->GetBlockHash()));
            // We should not violate GETDATA rules
            if (vToFetch.size() == MAX_INV_SZ) {
                LogPrintf("CFivegnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d blocks\n", pnode->id, MAX_INV_SZ);
                pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
                // Start filling new batch
                vToFetch.clear();
            }
        }
        if (!pindex->pprev) break;
        pindex = pindex->pprev;
    }

    std::map<int, CFivegnodeBlockPayees>::iterator it = mapFivegnodeBlocks.begin();

    while (it != mapFivegnodeBlocks.end()) {
        int nTotalVotes = 0;
        bool fFound = false;
        BOOST_FOREACH(CFivegnodePayee & payee, it->second.vecPayees)
        {
            if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
                fFound = true;
                break;
            }
            nTotalVotes += payee.GetVoteCount();
        }
        // A clear winner (MNPAYMENTS_SIGNATURES_REQUIRED+ votes) was found
        // or no clear winner was found but there are at least avg number of votes
        if (fFound || nTotalVotes >= (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2) {
            // so just move to the next block
            ++it;
            continue;
        }
        // DEBUG
//        DBG (
//            // Let's see why this failed
//            BOOST_FOREACH(CFivegnodePayee& payee, it->second.vecPayees) {
//                CTxDestination address1;
//                ExtractDestination(payee.GetPayee(), address1);
//                CBitcoinAddress address2(address1);
//                printf("payee %s votes %d\n", address2.ToString().c_str(), payee.GetVoteCount());
//            }
//            printf("block %d votes total %d\n", it->first, nTotalVotes);
//        )
        // END DEBUG
        // Low data block found, let's try to sync it
        uint256 hash;
        if (GetBlockHash(hash, it->first)) {
            vToFetch.push_back(CInv(MSG_FIVEGNODE_PAYMENT_BLOCK, hash));
        }
        // We should not violate GETDATA rules
        if (vToFetch.size() == MAX_INV_SZ) {
            LogPrintf("CFivegnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, MAX_INV_SZ);
            pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
            // Start filling new batch
            vToFetch.clear();
        }
        ++it;
    }
    // Ask for the rest of it
    if (!vToFetch.empty()) {
        LogPrintf("CFivegnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, vToFetch.size());
        pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
    }
}

std::string CFivegnodePayments::ToString() const {
    std::ostringstream info;

    info << "Votes: " << (int) mapFivegnodePaymentVotes.size() <<
         ", Blocks: " << (int) mapFivegnodeBlocks.size();

    return info.str();
}

bool CFivegnodePayments::IsEnoughData() {
    float nAverageVotes = (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2;
    int nStorageLimit = GetStorageLimit();
    return GetBlockCount() > nStorageLimit && GetVoteCount() > nStorageLimit * nAverageVotes;
}

int CFivegnodePayments::GetStorageLimit() {
    return std::max(int(mnodeman.size() * nStorageCoeff), nMinBlocksToStore);
}

void CFivegnodePayments::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
    LogPrint("mnpayments", "CFivegnodePayments::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);
    
    ProcessBlock(pindex->nHeight + 5);
}
