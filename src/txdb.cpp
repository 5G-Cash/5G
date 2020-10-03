// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

#include "chainparams.h"
#include "hash.h"
#include "pow.h"
#include "uint256.h"
#include "main.h"
#include "consensus/consensus.h"
#include "base58.h"

#include <stdint.h>

#include <boost/thread.hpp>

using namespace std;

static const char DB_COINS = 'c';
static const char DB_BLOCK_FILES = 'f';
static const char DB_TXINDEX = 't';
static const char DB_ADDRESSINDEX = 'a';
static const char DB_ADDRESSUNSPENTINDEX = 'u';
static const char DB_TIMESTAMPINDEX = 's';
static const char DB_SPENTINDEX = 'p';
static const char DB_BLOCK_INDEX = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_FLAG = 'F';
static const char DB_REINDEX_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';
static const char DB_TOTAL_SUPPLY = 'S';


CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe, true)
{
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) const {
    return db.Read(make_pair(DB_COINS, txid), coins);
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) const {
    return db.Exists(make_pair(DB_COINS, txid));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            if (it->second.coins.IsPruned())
                batch.Erase(make_pair(DB_COINS, it->first));
            else
                batch.Write(make_pair(DB_COINS, it->first), it->second.coins);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (!hashBlock.IsNull())
        batch.Write(DB_BEST_BLOCK, hashBlock);

    LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return db.WriteBatch(batch);
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, '1');
    else
        return Erase(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists(DB_REINDEX_FLAG);
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read(DB_LAST_BLOCK, nFile);
}

CCoinsViewCursor *CCoinsViewDB::Cursor() const
{
    CCoinsViewDBCursor *i = new CCoinsViewDBCursor(const_cast<CDBWrapper*>(&db)->NewIterator(), GetBestBlock());
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(DB_COINS);
    // Cache key of first record
    i->pcursor->GetKey(i->keyTmp);
    return i;
}

bool CCoinsViewDBCursor::GetKey(uint256 &key) const
{
    // Return cached key
    if (keyTmp.first == DB_COINS) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(CCoins &coins) const
{
    return pcursor->GetValue(coins);
}

unsigned int CCoinsViewDBCursor::GetValueSize() const
{
    return pcursor->GetValueSize();
}

bool CCoinsViewDBCursor::Valid() const
{
    return keyTmp.first == DB_COINS;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();
    if (!pcursor->Valid() || !pcursor->GetKey(keyTmp))
        keyTmp.first = 0; // Invalidate cached key after last record so that Valid() and GetKey() return false
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(make_pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockIndex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
    	batch.Write(make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()), CDiskBlockIndex(*it));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(make_pair(DB_TXINDEX, txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(make_pair(DB_TXINDEX, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value) {
    return Read(make_pair(DB_SPENTINDEX, key), value);
}

bool CBlockTreeDB::UpdateSpentIndex(const std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CSpentIndexKey,CSpentIndexValue> >::const_iterator it=vect.begin(); it!=vect.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(make_pair(DB_SPENTINDEX, it->first));
        } else {
            batch.Write(make_pair(DB_SPENTINDEX, it->first), it->second);
        }
    }
    return WriteBatch(batch);
}

bool CBlockTreeDB::UpdateAddressUnspentIndex(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=vect.begin(); it!=vect.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(make_pair(DB_ADDRESSUNSPENTINDEX, it->first));
        } else {
            batch.Write(make_pair(DB_ADDRESSUNSPENTINDEX, it->first), it->second);
        }
    }
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadAddressUnspentIndex(uint160 addressHash, AddressType type,
                                           std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs) {

    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_ADDRESSUNSPENTINDEX, CAddressIndexIteratorKey(type, addressHash)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressUnspentKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSUNSPENTINDEX && key.second.hashBytes == addressHash) {
            CAddressUnspentValue nValue;
            if (pcursor->GetValue(nValue)) {
                unspentOutputs.push_back(make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address unspent value");
            }
        } else {
            break;
        }
    }

    return true;
}

bool CBlockTreeDB::WriteAddressIndex(const std::vector<std::pair<CAddressIndexKey, CAmount > >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
    batch.Write(make_pair(DB_ADDRESSINDEX, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::EraseAddressIndex(const std::vector<std::pair<CAddressIndexKey, CAmount > >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
    batch.Erase(make_pair(DB_ADDRESSINDEX, it->first));
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadAddressIndex(uint160 addressHash, AddressType type,
                                    std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,
                                    int start, int end) {

    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    if (start > 0 && end > 0) {
        pcursor->Seek(make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorHeightKey(type, addressHash, start)));
    } else {
        pcursor->Seek(make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorKey(type, addressHash)));
    }

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressIndexKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSINDEX && key.second.hashBytes == addressHash && key.second.type == type) {
            if (end > 0 && key.second.blockHeight > end) {
                break;
            }
            CAmount nValue;
            if (pcursor->GetValue(nValue)) {
                addressIndex.push_back(make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address index value");
            }
        } else {
            break;
        }
    }

    return true;
}


bool CBlockTreeDB::WriteTimestampIndex(const CTimestampIndexKey &timestampIndex) {
    CDBBatch batch(*this);
    batch.Write(make_pair(DB_TIMESTAMPINDEX, timestampIndex), 0);
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadTimestampIndex(const unsigned int &high, const unsigned int &low, std::vector<uint256> &hashes) {

    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_TIMESTAMPINDEX, CTimestampIndexIteratorKey(low)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, CTimestampIndexKey> key;
        if (pcursor->GetKey(key) && key.first == DB_TIMESTAMPINDEX && key.second.timestamp <= high) {
            hashes.push_back(key.second.blockHash);
            pcursor->Next();
        } else {
            break;
        }
    }

    return true;
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair(DB_FLAG, name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts(boost::function<CBlockIndex*(const uint256&)> insertBlockIndex)
{
    auto consensusParams = Params().GetConsensus();
    LogPrintf("CBlockTreeDB::LoadBlockIndexGuts\n");
    //bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_BLOCK_INDEX, uint256()));

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX) {
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex)) {
                // Construct block index object
            	//if(diskindex.hashBlock != uint256()
            	//	&& diskindex.hashPrev != uint256()){

                CBlockIndex* pindexNew    = insertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev 		  = insertBlockIndex(diskindex.hashPrev);

                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->nBits          = diskindex.nBits;
                pindexNew->nNonce         = diskindex.nNonce;
                pindexNew->nStatus        = diskindex.nStatus;
                pindexNew->nTx            = diskindex.nTx;

                pindexNew->accumulatorChanges = diskindex.accumulatorChanges;
                pindexNew->mintedPubCoins     = diskindex.mintedPubCoins;
                pindexNew->spentSerials       = diskindex.spentSerials;

                pindexNew->sigmaMintedPubCoins   = diskindex.sigmaMintedPubCoins;
                pindexNew->sigmaSpentSerials     = diskindex.sigmaSpentSerials;
                pindexNew->nStakeModifier = diskindex.nStakeModifier;
                pindexNew->vchBlockSig    = diskindex.vchBlockSig; // qtum

                if (pindexNew->nNonce != 0 && !CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits, consensusParams))
                        return error("LoadBlockIndex(): CheckProofOfWork failed: %s", pindexNew->ToString());

                pcursor->Next();
            } else {
                return error("LoadBlockIndex() : failed to read value");
            }
        } else {
            break;
        }
    }

    return true;
}

int CBlockTreeDB::GetBlockIndexVersion()
{
    // Get random block index entry, check its version. The only reason for these functions to exist
    // is to check if the index is from previous version and needs to be rebuilt. Comparison of ANY
    // record version to threshold value would be enough to decide if reindex is needed.

    return GetBlockIndexVersion(uint256());
}

int CBlockTreeDB::GetBlockIndexVersion(uint256 const & blockHash)
{
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(make_pair(DB_BLOCK_INDEX, blockHash));
    uint256 const zero_hash = uint256();
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX) {
            if (blockHash != zero_hash && key.second != blockHash) {
                pcursor->Next();
                continue;
            }
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex))
                return diskindex.nDiskBlockVersion;
        } else {
	    break;
        }
    }
    return -1;
}


bool CBlockTreeDB::AddTotalSupply(CAmount const & supply)
{
    CAmount current = 0;
    Read(DB_TOTAL_SUPPLY, current);
    current += supply;
    return Write(DB_TOTAL_SUPPLY, current);
}

bool CBlockTreeDB::ReadTotalSupply(CAmount & supply)
{
    CAmount current = 0;
    if(Read(DB_TOTAL_SUPPLY, current)) {
        supply = current;
        return true;
    }
    return false;
}

/******************************************************************************/

CDbIndexHelper::CDbIndexHelper(bool addressIndex_, bool spentIndex_)
{
    if (addressIndex_) {
        addressIndex.reset(AddressIndex());
        addressUnspentIndex.reset(AddressUnspentIndex());
    }

    if (spentIndex_)
        spentIndex.reset(SpentIndex());
}

namespace {

using AddressIndexPtr = boost::optional<CDbIndexHelper::AddressIndex>;
using AddressUnspentIndexPtr = boost::optional<CDbIndexHelper::AddressUnspentIndex>;
using SpentIndexPtr = boost::optional<CDbIndexHelper::SpentIndex>;

std::pair<AddressType, uint160> classifyAddress(txnouttype type, vector<vector<unsigned char> > const & addresses)
{
    std::pair<AddressType, uint160> result(AddressType::unknown, uint160());
    if(type == TX_PUBKEY) {
        result.first = AddressType::payToPubKeyHash;
        CPubKey pubKey(addresses.front().begin(), addresses.front().end());
        result.second = pubKey.GetID();
    } else if(type == TX_SCRIPTHASH) {
        result.first = AddressType::payToScriptHash;
        result.second = uint160(std::vector<unsigned char>(addresses.front().begin(), addresses.front().end()));
    } else if(type == TX_PUBKEYHASH) {
        result.first = AddressType::payToPubKeyHash;
        result.second = uint160(std::vector<unsigned char>(addresses.front().begin(), addresses.front().end()));
    }
    return result;
}

void handleInput(CTxIn const & input, size_t inputNo, uint256 const & txHash, int height, int txNumber, CCoinsViewCache const & view,
        AddressIndexPtr & addressIndex, AddressUnspentIndexPtr & addressUnspentIndex, SpentIndexPtr & spentIndex)
{
    const CCoins* coins = view.AccessCoins(input.prevout.hash);
    const CTxOut &prevout = coins->vout[input.prevout.n];

    txnouttype type;
    vector<vector<unsigned char> > addresses;

    if(!Solver(prevout.scriptPubKey, type, addresses)) {
        LogPrint("CDbIndexHelper", "Encountered an unsoluble script in block:%i, txHash: %s, inputNo: %i\n", height, txHash.ToString().c_str(), inputNo);
        return;
    }

    std::pair<AddressType, uint160> addrType = classifyAddress(type, addresses);

    if(addrType.first == AddressType::unknown) {
        return;
    }

    if (addressIndex) {
        addressIndex->push_back(make_pair(CAddressIndexKey(addrType.first, addrType.second, height, txNumber, txHash, inputNo, true), prevout.nValue * -1));
        addressUnspentIndex->push_back(make_pair(CAddressUnspentKey(addrType.first, addrType.second, input.prevout.hash, input.prevout.n), CAddressUnspentValue()));
    }

    if (spentIndex)
        spentIndex->push_back(make_pair(CSpentIndexKey(input.prevout.hash, input.prevout.n), CSpentIndexValue(txHash, inputNo, height, prevout.nValue, addrType.first, addrType.second)));
}

void handleRemint(CTxIn const & input, uint256 const & txHash, int height, int txNumber, CAmount nValue,
        AddressIndexPtr & addressIndex, AddressUnspentIndexPtr & addressUnspentIndex, SpentIndexPtr & spentIndex)
{
    if(!input.IsZerocoinRemint())
        return;

    if (addressIndex) {
        addressIndex->push_back(make_pair(CAddressIndexKey(AddressType::zerocoinRemint, uint160(), height, txNumber, txHash, 0, true), nValue * -1));
        addressUnspentIndex->push_back(make_pair(CAddressUnspentKey(AddressType::zerocoinRemint, uint160(), input.prevout.hash, input.prevout.n), CAddressUnspentValue()));
    }

    if (spentIndex)
        spentIndex->push_back(make_pair(CSpentIndexKey(input.prevout.hash, input.prevout.n), CSpentIndexValue(txHash, 0, height, nValue, AddressType::zerocoinRemint, uint160())));
}


template <class Iterator>
void handleZerocoinSpend(Iterator const begin, Iterator const end, uint256 const & txHash, int height, int txNumber, CCoinsViewCache const & view,
        AddressIndexPtr & addressIndex, bool isV3)
{
    if(!addressIndex)
        return;

    CAmount spendAmount = 0;
    for(Iterator iter = begin; iter != end; ++iter)
        spendAmount += iter->nValue;

    addressIndex->push_back(make_pair(CAddressIndexKey(isV3 ? AddressType::sigmaSpend : AddressType::zerocoinSpend, uint160(), height, txNumber, txHash, 0, true), -spendAmount));
}

void handleOutput(const CTxOut &out, size_t outNo, uint256 const & txHash, int height, int txNumber, CCoinsViewCache const & view, bool coinbase,
        AddressIndexPtr & addressIndex, AddressUnspentIndexPtr & addressUnspentIndex, SpentIndexPtr & spentIndex)
{
    if(!addressIndex)
        return;

    if(out.scriptPubKey.IsZerocoinMint())
        addressIndex->push_back(make_pair(CAddressIndexKey(AddressType::zerocoinMint, uint160(), height, txNumber, txHash, outNo, false), out.nValue));

    if(out.scriptPubKey.IsSigmaMint())
        addressIndex->push_back(make_pair(CAddressIndexKey(AddressType::sigmaMint, uint160(), height, txNumber, txHash, outNo, false), out.nValue));

    txnouttype type;
    vector<vector<unsigned char> > addresses;

    if(!Solver(out.scriptPubKey, type, addresses)) {
        LogPrint("CDbIndexHelper", "Encountered an unsoluble script in block:%i, txHash: %s, outNo: %i\n", height, txHash.ToString().c_str(), outNo);
        return;
    }

    std::pair<AddressType, uint160> addrType = classifyAddress(type, addresses);

    if(addrType.first == AddressType::unknown) {
        return;
    }

    addressIndex->push_back(make_pair(CAddressIndexKey(addrType.first, addrType.second, height, txNumber, txHash, outNo, false), out.nValue));
    addressUnspentIndex->push_back(make_pair(CAddressUnspentKey(addrType.first, addrType.second, txHash, outNo), CAddressUnspentValue(out.nValue, out.scriptPubKey, height)));
}
}


void CDbIndexHelper::ConnectTransaction(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view)
{
    size_t no = 0;
    if(!tx.IsCoinBase() && !tx.IsZerocoinSpend() && !tx.IsSigmaSpend() && !tx.IsZerocoinRemint()) {
        for (CTxIn const & input : tx.vin) {
            handleInput(input, no++, tx.GetHash(), height, txNumber, view, addressIndex, addressUnspentIndex, spentIndex);
        }
    }

    if(tx.IsZerocoinRemint()) {
        CAmount remintValue = 0;
        for (CTxOut const & out : tx.vout) {
            remintValue += out.nValue;
        }
        if (tx.vin.size() != 1) {
           error("A Zerocoin to Sigma remint tx shoud have just 1 input");
           return;
        }
        handleRemint(tx.vin[0], tx.GetHash(), height, txNumber, remintValue, addressIndex, addressUnspentIndex, spentIndex);
    }

    if(tx.IsZerocoinSpend() || tx.IsSigmaSpend())
        handleZerocoinSpend(tx.vout.begin(), tx.vout.end(), tx.GetHash(), height, txNumber, view, addressIndex, tx.IsSigmaSpend());

    no = 0;
    bool const txIsCoinBase = tx.IsCoinBase();
    for (CTxOut const & out : tx.vout) {
        handleOutput(out, no++, tx.GetHash(), height, txNumber, view, txIsCoinBase, addressIndex, addressUnspentIndex, spentIndex);
    }
}


void CDbIndexHelper::DisconnectTransactionInputs(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view)
{
    size_t pAddressBegin{0}, pUnspentBegin{0}, pSpentBegin{0};

    if(addressIndex){
        pAddressBegin = addressIndex->size();
        pUnspentBegin = addressUnspentIndex->size();
    }

    if(spentIndex)
        pSpentBegin = spentIndex->size();

    if(tx.IsZerocoinRemint()) {
        CAmount remintValue = 0;
        for (CTxOut const & out : tx.vout) {
            remintValue += out.nValue;
        }
        if (tx.vin.size() != 1) {
           error("A Zerocoin to Sigma remint tx shoud have just 1 input");
           return;
        }
        handleRemint(tx.vin[0], tx.GetHash(), height, txNumber, remintValue, addressIndex, addressUnspentIndex, spentIndex);
    }

    size_t no = 0;

    if(!tx.IsCoinBase() && !tx.IsZerocoinSpend() && !tx.IsSigmaSpend() && !tx.IsZerocoinRemint())
        for (CTxIn const & input : tx.vin) {
            handleInput(input, no++, tx.GetHash(), height, txNumber, view, addressIndex, addressUnspentIndex, spentIndex);
        }

    if(addressIndex){
        std::reverse(addressIndex->begin() + pAddressBegin, addressIndex->end());
        std::reverse(addressUnspentIndex->begin() + pUnspentBegin, addressUnspentIndex->end());

        for(AddressUnspentIndex::iterator iter = addressUnspentIndex->begin(); iter != addressUnspentIndex->end(); ++iter)
            iter->second = CAddressUnspentValue();
    }

    if(spentIndex)
        std::reverse(spentIndex->begin() + pSpentBegin, spentIndex->end());
}

void CDbIndexHelper::DisconnectTransactionOutputs(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view)
{
    if(tx.IsZerocoinSpend() || tx.IsSigmaSpend())
        handleZerocoinSpend(tx.vout.begin(), tx.vout.end(), tx.GetHash(), height, txNumber, view, addressIndex, tx.IsSigmaSpend());

    size_t no = 0;
    bool const txIsCoinBase = tx.IsCoinBase();
    for (CTxOut const & out : tx.vout) {
        handleOutput(out, no++, tx.GetHash(), height, txNumber, view, txIsCoinBase, addressIndex, addressUnspentIndex, spentIndex);
    }

    if(addressIndex)
    {
        std::reverse(addressIndex->begin(), addressIndex->end());
        std::reverse(addressUnspentIndex->begin(), addressUnspentIndex->end());
    }

    if(spentIndex)
        std::reverse(spentIndex->begin(), spentIndex->end());
}

CDbIndexHelper::AddressIndex const & CDbIndexHelper::getAddressIndex() const
{
    return *addressIndex;
}


CDbIndexHelper::AddressUnspentIndex const & CDbIndexHelper::getAddressUnspentIndex() const
{
    return *addressUnspentIndex;
}


CDbIndexHelper::SpentIndex const & CDbIndexHelper::getSpentIndex() const
{
    return *spentIndex;
}
