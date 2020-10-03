// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include "amount.h"
#include "../libzerocoin/bitcoin_bignum/bignum.h"
#include "../sigma/coin.h"
#include "streams.h"
#include "tinyformat.h"
#include "ui_interface.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "script/ismine.h"
#include "wallet/crypter.h"
#include "wallet/walletdb.h"
#include "wallet/rpcwallet.h"
#include "pos.h"
#include "wallet/mnemoniccontainer.h"
#include "../base58.h"
#include "zerocoin_params.h"
#include "univalue.h"

#include "hdmint/tracker.h"
#include "hdmint/wallet.h"

#include "primitives/zerocoin.h"


#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/shared_ptr.hpp>

extern CWallet* pwalletMain;
extern CHDMintWallet* zwalletMain;

/**
 * Settings
 */
extern CFeeRate payTxFee;
extern CAmount nReserveBalance;
extern unsigned int nTxConfirmTarget;
extern bool bSpendZeroConfChange;
extern bool fSendFreeTransactions;
extern bool fWalletUnlockStakingOnly;

static const unsigned int DEFAULT_KEYPOOL_SIZE = 100;
//! -paytxfee default
static const CAmount DEFAULT_TRANSACTION_FEE = 0;
//! -fallbackfee default
static const CAmount DEFAULT_FALLBACK_FEE = 20000;
//! -mintxfee default
static const CAmount DEFAULT_TRANSACTION_MINFEE = 1000;
//! minimum change amount
static const CAmount MIN_CHANGE = CENT;
//! Default for -spendzeroconfchange
static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//! Default for -sendfreetransactions
static const bool DEFAULT_SEND_FREE_TRANSACTIONS = false;
//! Default for -walletrejectlongchains
static const bool DEFAULT_WALLET_REJECT_LONG_CHAINS = false;
//! -txconfirmtarget default
static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 2;
//! Largest (in bytes) free transaction we're willing to create
static const unsigned int MAX_FREE_TRANSACTION_CREATE_SIZE = 1000;
static const bool DEFAULT_WALLETBROADCAST = true;

static bool DEFAULT_UPGRADE_CHAIN = false;

//! if set, all keys will be derived by using BIP32
static const bool DEFAULT_USE_HD_WALLET = true;

//! if set, all keys will be derived by using BIP39
static const bool DEFAULT_USE_MNEMONIC = true;

extern const char * DEFAULT_WALLET_DAT;

const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;
const uint32_t BIP44_INDEX = 0x2C;
const uint32_t BIP44_TEST_INDEX = 0x1;   // https://github.com/satoshilabs/slips/blob/master/slip-0044.md#registered-coin-types
const uint32_t BIP44_ZCOIN_INDEX = 0x1DB; // https://github.com/satoshilabs/slips/blob/master/slip-0044.md#registered-coin-types
const uint32_t BIP44_MINT_INDEX = 0x2;
#ifdef ENABLE_ELYSIUM
const uint32_t BIP44_ELYSIUM_MINT_INDEX_V0 = 0x3;
const uint32_t BIP44_ELYSIUM_MINT_INDEX_V1 = 0x4;
#endif

class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CTxMemPool;
class CWalletTx;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_HD = 130000, // Hierarchical key derivation after BIP32 (HD Wallet)
    FEATURE_LATEST = FEATURE_COMPRPUBKEY // HD is optional, use FEATURE_COMPRPUBKEY as latest version
};

enum AvailableCoinsType
{
    ALL_COINS = 1,
    ONLY_DENOMINATED = 2,
    ONLY_NOT1000IFMN = 3,
    ONLY_NONDENOMINATED_NOT1000IFMN = 4,
    ONLY_1000 = 5, // find fivegnode outputs including locked ones (use with caution)
    ONLY_PRIVATESEND_COLLATERAL = 6,
    ONLY_MINTS = 7,
    WITH_MINTS = 8
};

struct CompactTallyItem
{
    CBitcoinAddress address;
    CAmount nAmount;
    std::vector<CTxIn> vecTxIn;
    CompactTallyItem()
    {
        nAmount = 0;
    }
};


/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
    }
};

/** Address book data */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData()
    {
        purpose = "unknown";
    }

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient
{
    CScript scriptPubKey;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;
};

typedef std::map<std::string, std::string> mapValue_t;


static void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
  /** Constant used in hashBlock to indicate tx has been abandoned */
    static const uint256 ABANDON_HASH;

public:
    uint256 hashBlock;

    /* An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */
    int nIndex;

    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        std::vector<uint256> vMerkleBranch; // For compatibility with older versions.
        READWRITE(*(CTransaction*)this);
        nVersion = this->nVersion;
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    int SetMerkleBranch(const CBlock& block);

    /**
     * Return depth of transaction in blockchain:
     * <0  : conflicts with a transaction this deep in the blockchain
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(const CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }

    int GetDepthInMainChain(const CBlockIndex* &pindexRet, bool enableIX) const;
    int GetDepthInMainChain(bool enableIX) const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet, enableIX); }

    bool IsInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet) > 0; }
    int GetBlocksToMaturity() const;

    /** Pass this transaction to the mempool. Fails if absolute fee exceeds absurd fee. */
    bool AcceptToMemoryPool(
        bool fLimitFree,
        const CAmount nAbsurdFee,
        CValidationState& state,
        bool fCheckInputs,
        bool isCheckWalletTransaction = false,
        bool markZcoinSpendTransactionSerial = true);

    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }
};

/**
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //!< time received by this node
    unsigned int nTimeSmart;
    char fFromMe;
    std::string strFromAccount;
    int64_t nOrderPos; //!< position in ordered transaction list
    std::unordered_set<uint32_t> changes; //!< positions of changes in vout

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fImmatureStakeCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nImmatureStakeCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(const CWallet* pwalletIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fImmatureStakeCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nImmatureStakeCreditCached = 0;
        nAvailableCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
        changes.clear();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        constexpr uint32_t FLAG_WITH_CHANGES = 0x00000001;

        if (ser_action.ForRead())
            Init(NULL);

        char fSpent = false;
        uint32_t flags = 0;

        if (!ser_action.ForRead())
        {
            flags = FLAG_WITH_CHANGES;

            mapValue["fromaccount"] = strFromAccount;
            mapValue["flags"] = strprintf("0x%x", flags);

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //!< Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead())
        {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;

            auto it = mapValue.find("flags");
            if (it != mapValue.end()) {
                flags = static_cast<uint32_t>(std::strtoul(it->second.c_str(), nullptr, 0));
            }
        }

        if (flags & FLAG_WITH_CHANGES) {
            READWRITE(changes);
        }

        mapValue.erase("fromaccount");
        mapValue.erase("version");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
        mapValue.erase("flags");
    }

    //! make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(bool fUseCache=true) const;
    CAmount GetImmatureStakeCredit(bool fUseCache=true) const;
    CAmount GetAvailableCredit(bool fUseCache=true, bool fExcludeLocked = false) const;
    CAmount GetImmatureWatchOnlyCredit(const bool& fUseCache=true) const;
    CAmount GetAvailableWatchOnlyCredit(const bool& fUseCache=true) const;
    CAmount GetAnonymizedCredit(bool fUseCache=true) const;
    CAmount GetChange() const;

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, CAmount& nFee, std::string& strSentAccount, const isminefilter& filter) const;

    void GetAccountAmounts(const std::string& strAccount, CAmount& nReceived,
                           CAmount& nSent, CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const
    {
        return (GetDebit(filter) > 0);
    }

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool InMempool() const;
    bool InStempool() const;
    bool IsTrusted() const;

    bool IsChange(uint32_t out) const;
    bool IsChange(const CTxOut& out) const;

    int64_t GetTxTime() const;
    int GetRequestCount() const;

    bool RelayWalletTransaction(bool fCheckInputs = true);

    std::set<uint256> GetConflicts() const;
};




class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;
    bool fSpendable;
    bool fSolvable;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn, bool fSolvableIn)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn; fSpendable = fSpendableIn; fSolvable = fSolvableIn;
    }
    int Priority() const;

    std::string ToString() const;
};




/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires=0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};

/**
 * Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    CAmount nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64_t nOrderPos; //!< position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
        nEntryNo = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead())
        {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty()))
            {
                CDataStream ss(nType, nVersion);
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead())
        {
            mapValue.clear();
            if (std::string::npos != nSepPos)
            {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), nType, nVersion);
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

enum MintAlgorithm {
    ZEROCOIN = 1,
    SIGMA = 2
};

/**
 * A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore, public CValidationInterface
{
private:
    /**
     * Select a set of coins such that nValueRet >= nTargetValue and at least
     * all coins from coinControl are selected; Never select unconfirmed coins
     * if they are not ours
     */
    bool SelectCoins(const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl *coinControl = NULL, AvailableCoinsType nCoinType = ALL_COINS, bool fUseInstantSend = false) const;

    CWalletDB *pwalletdbEncryption;

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

    int64_t nNextResend;
    int64_t nLastResend;
    bool fBroadcastTransactions;
    std::map<COutPoint, CStakeCache> stakeCache;

    mutable bool fAnonymizableTallyCached;
    mutable std::vector<CompactTallyItem> vecAnonymizableTallyCached;
    mutable bool fAnonymizableTallyCachedNonDenom;
    mutable std::vector<CompactTallyItem> vecAnonymizableTallyCachedNonDenom;

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef std::multimap<COutPoint, uint256> TxSpends;
    TxSpends mapTxSpends;
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);
	void RemoveFromSpends(const COutPoint& outpoint, const uint256& wtxid);
    void RemoveFromSpends(const uint256& wtxid);
    /* Mark a transaction (and its in-wallet descendants) as conflicting with a particular block. */
    void MarkConflicted(const uint256& hashBlock, const uint256& hashTx);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

    /* the HD chain data model (external chain counters) */
    CHDChain hdChain;
    MnemonicContainer mnemonicContainer;

public:
    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable CCriticalSection cs_wallet;

    bool fFileBacked;
    std::string strWalletFile;

    std::set<int64_t> setKeyPool;
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;
    //fivegnode
    int64_t nKeysLeftSinceAutoBackup;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    CWallet()
    {
        SetNull();
    }

    CWallet(const std::string& strWalletFileIn)
    {
        SetNull();

        strWalletFile = strWalletFileIn;
        fFileBacked = true;
    }

    ~CWallet()
    {
        delete pwalletdbEncryption;
        pwalletdbEncryption = NULL;
    }

    void SetNull()
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nNextResend = 0;
        nLastResend = 0;
        nTimeFirstKey = 0;
        fBroadcastTransactions = false;
        fAnonymizableTallyCached = false;
        fAnonymizableTallyCachedNonDenom = false;
        vecAnonymizableTallyCached.clear();
        vecAnonymizableTallyCachedNonDenom.clear();
    }

    std::map<uint256, CWalletTx> mapWallet;
    std::list<CAccountingEntry> laccentries;
    bool EraseFromWallet(uint256 hash);
    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems wtxOrdered;

    int64_t nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<CTxDestination, CAddressBookData> mapAddressBook;

    CPubKey vchDefaultKey;

    std::set<COutPoint> setLockedCoins;

    int64_t nTimeFirstKey;

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; }

    /**
     * populate vCoins with vector of available COutputs.
     */
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl = NULL, bool fIncludeZeroValue=false, AvailableCoinsType nCoinType=ALL_COINS, bool fUseInstantSend = false) const;

    bool IsHDSeedAvailable() { return !hdChain.masterKeyID.IsNull(); }

    /**
     * Shuffle and select coins until nTargetValue is reached while avoiding
     * small change; This method is stochastic for some inputs and upon
     * completion the coin set and corresponding actual target value is
     * assembled
     */
    bool SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, uint64_t nMaxAncestors, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet) const;
    bool SelectCoinsByDenominations(int nDenom, CAmount nValueMin, CAmount nValueMax, std::vector<CTxIn>& vecTxInRet, std::vector<COutput>& vCoinsRet, CAmount& nValueRet, int nPrivateSendRoundsMin, int nPrivateSendRoundsMax);
    bool GetCollateralTxIn(CTxIn& txinRet, CAmount& nValueRet) const;
    bool SelectCoinsDark(CAmount nValueMin, CAmount nValueMax, std::vector<CTxIn>& vecTxInRet, CAmount& nValueRet, int nPrivateSendRoundsMin, int nPrivateSendRoundsMax) const;
    bool SelectCoinsGrouppedByAddresses(std::vector<CompactTallyItem>& vecTallyRet, bool fSkipDenominated = true, bool fAnonymizable = true) const;

    bool IsSpent(const uint256& hash, unsigned int n) const;

    bool IsLockedCoin(uint256 hash, unsigned int n) const;
    void LockCoin(const COutPoint& output);
    void UnlockCoin(const COutPoint& output);
    void UnlockAllCoins();
    void ListLockedCoins(std::vector<COutPoint>& vOutpts);

    // fivegnode
    /// Get 10000 FIVEG output and keys which can be used for the Fivegnode
    bool GetFivegnodeVinAndKeys(CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, std::string strTxHash = "", std::string strOutputIndex = "");
    /// Extract txin information and keys from output
    bool GetVinAndKeysFromOutput(COutput out, CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet);
    bool HasCollateralInputs(bool fOnlyConfirmed = true) const;
    int  CountInputsWithAmount(CAmount nInputAmount);

    CPubKey GetKeyFromKeypath(uint32_t nChange, uint32_t nChild);
    /**
     * keystore implementation
     * Generate a new key
     */
    CPubKey GenerateNewKey(uint32_t nChange=0);
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey &pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
    //! Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &metadata);

    bool LoadMinVersion(int nVersion) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);

    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
    //! Adds a destination data tuple to the store, without saving it to disk
    bool LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Look up a destination data tuple in the store, return true if found false otherwise
    bool GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const;

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript &dest);
    bool RemoveWatchOnly(const CScript &dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript &dest);

    bool Unlock(const SecureString& strWalletPassphrase, const bool& fFirstUnlock=false);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const;

    /**
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB *pwalletdb = NULL);
    bool AccountMove(std::string strFrom, std::string strTo, CAmount nAmount, std::string strComment = "");
    bool GetAccountPubkey(CPubKey &pubKey, std::string strAccount, bool bForceNew = false);

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb);
    void SyncTransaction(const CTransaction& tx, const CBlockIndex *pindex, const CBlock* pblock);
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate);
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    void ReacceptWalletTransactions();
    void ResendWalletTransactions(int64_t nBestBlockTime);
    std::vector<uint256> ResendWalletTransactionsBefore(int64_t nTime);
    //fivegnode
    CAmount GetBalance(bool fExcludeLocked = false) const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    CAmount GetStake() const;
    CAmount GetWatchOnlyStake() const;
    CAmount GetWatchOnlyBalance() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;
    // get the PrivateSend chain depth for a given input
    int GetRealInputPrivateSendRounds(CTxIn txin, int nRounds) const;
    // respect current settings
    int GetInputPrivateSendRounds(CTxIn txin) const;
    bool IsDenominated(const CTxIn &txin) const;
    bool IsDenominatedAmount(CAmount nInputAmount) const;
    bool IsCollateralAmount(CAmount nInputAmount) const;
    CAmount GetAnonymizableBalance(bool fSkipDenominated = false) const;
    CAmount GetAnonymizedBalance() const;
//    double GetAverageAnonymizedRounds() const;
//    CAmount GetNormalizedAnonymizedBalance() const;
    CAmount GetNeedsToBeAnonymizedBalance(CAmount nMinBalance = 0) const;
    CAmount GetDenominatedBalance(bool unconfirmed=false) const;

    static std::vector<CRecipient> CreateSigmaMintRecipients(
        std::vector<sigma::PrivateCoin>& coins,
        vector<CHDMint>& vDMints);


    static int GetRequiredCoinCountForAmount(
        const CAmount& required,
        const std::vector<sigma::CoinDenomination>& denominations);

    static CAmount SelectMintCoinsForAmount(
        const CAmount& required,
        const std::vector<sigma::CoinDenomination>& denominations,
        std::vector<sigma::CoinDenomination>& coinsOut);

    static CAmount SelectSpendCoinsForAmount(
        const CAmount& required,
        const std::list<CSigmaEntry>& coinsIn,
        std::vector<CSigmaEntry>& coinsOut);

    // Returns a list of unspent and verified coins, I.E. coins which are ready
    // to be spent.
    std::list<CSigmaEntry> GetAvailableCoins(const CCoinControl *coinControl = NULL, bool includeUnsafe = false, bool fDummy = false) const;

    /** \brief Selects coins to spend, and coins to re-mint based on the required amount to spend, provided by the user. As the lower denomination now is 0.1 index, user's request will be rounded up to the nearest 0.1. This difference between the user's requested value, and the actually spent value will be left to the miners as a fee.
     * \param[in] required Required amount to spend.
     * \param[out] coinsToSpend_out Coins which user needs to spend.
     * \param[out] coinsToMint_out Coins which will be re-minted by the user to get the change back.
     * \returns true, if it was possible to spend exactly required(rounded up to 0.1 index) amount using coins we have.
     */
    bool GetCoinsToSpend(
        CAmount required,
        std::vector<CSigmaEntry>& coinsToSpend_out,
        std::vector<sigma::CoinDenomination>& coinsToMint_out,
        const size_t coinsLimit = SIZE_MAX,
        const CAmount amountLimit = MAX_MONEY,
        const CCoinControl *coinControl = NULL,
        bool fDummy = false) const;

    /**
     * Insert additional inputs into the transaction by
     * calling CreateTransaction();
     */
    bool FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, bool overrideEstimatedFeeRate, const CFeeRate& specificFeeRate, int& nChangePosInOut, std::string& strFailReason, bool includeWatching, bool lockUnspents, const CTxDestination& destChange = CNoDestination());

    /**
     * Create a new transaction paying the recipients with a set of coins
     * selected by SelectCoins(); Also create the change output, when needed
     * @note passing nChangePosInOut as -1 will result in setting a random position
     */
    bool CreateTransaction(const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosInOut,
                           std::string& strFailReason, const CCoinControl *coinControl = NULL, bool sign = true, AvailableCoinsType nCoinType = ALL_COINS, bool fUseInstantSend = false);

    /**
     * Add zerocoin Mint and Spend function
     */
    bool IsMintFromTxOutAvailable(CTxOut txout, bool& fIsAvailable);
    void ListAvailableCoinsMintCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true) const;

    bool GetTxInfoForPubcoin(const CZerocoinEntry &pubCoinItem, std::string& fTxid, unsigned int& fIndex) const;
    
    void ListAvailableSigmaMintCoins(vector <COutput> &vCoins, bool fOnlyConfirmed) const;

    bool CreateZerocoinMintTransaction(const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosInOut,
                           std::string& strFailReason, bool isSigmaMint, const CCoinControl *coinControl = NULL, bool sign = true);
    bool CreateZerocoinMintTransaction(CScript pubCoin, int64_t nValue,
                                       CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, std::string& strFailReason, bool isSigmaMint, const CCoinControl *coinControl=NULL);
    bool CreateZerocoinSpendTransaction(std::string& thirdPartyaddress, int64_t nValue, libzerocoin::CoinDenomination denomination,
                                        CWalletTx& wtxNew, CReserveKey& reservekey, CBigNum& coinSerial, uint256& txHash, CBigNum& zcSelectedValue, bool& zcSelectedIsUsed,  std::string& strFailReason, bool forceUsed = false);

    bool CreateSigmaSpendTransaction(
        std::string& thirdPartyaddress,
        sigma::CoinDenomination denomination,
        CWalletTx& wtxNew,
        CReserveKey& reservekey,
        CAmount& nFeeRet,
        Scalar& coinSerial,
        uint256& txHash,
        GroupElement& zcSelectedValue,
        bool& zcSelectedIsUsed,
        std::string& strFailReason,
        bool forceUsed = false,
        const CCoinControl *coinControl = NULL);

    CWalletTx CreateSigmaSpendTransaction(
        const std::vector<CRecipient>& recipients,
        CAmount& fee,
        std::vector<CSigmaEntry>& selected,
        std::vector<CHDMint>& changes,
        bool& fChangeAddedToFee,
        const CCoinControl *coinControl = NULL,
        bool fDummy = false);

    bool CreateMultipleZerocoinSpendTransaction(std::string& thirdPartyaddress, const std::vector<std::pair<int64_t, libzerocoin::CoinDenomination>>& denominations,
                                        CWalletTx& wtxNew, CReserveKey& reservekey, vector<CBigNum>& coinSerials, uint256& txHash, vector<CBigNum>& zcSelectedValues, std::string& strFailReason, bool forceUsed = false);
    bool CreateMultipleSigmaSpendTransaction(
        std::string& thirdPartyaddress,
        const std::vector<sigma::CoinDenomination>& denominations,
        CWalletTx& wtxNew,
        CReserveKey& reservekey,
        CAmount& nFeeRet,
        vector<Scalar>& coinSerials,
        uint256& txHash,
        vector<GroupElement>& zcSelectedValues,
        std::string& strFailReason,       
        bool forceUsed = false,
        const CCoinControl *coinControl = NULL);

    bool CommitZerocoinSpendTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);
    bool CommitSigmaTransaction(CWalletTx& wtxNew, std::vector<CSigmaEntry>& selectedCoins, std::vector<CHDMint>& changes);
    std::string SendMoney(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);
    std::string SendMoneyToDestination(const CTxDestination &address, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);

    std::string MintZerocoin(CScript pubCoin, int64_t nValue, bool isSigmaMint, CWalletTx& wtxNew, bool fAskFee=false);
    std::string MintAndStoreZerocoin(vector<CRecipient> vecSend, vector<libzerocoin::PrivateCoin> privCoins, CWalletTx &wtxNew, bool fAskFee=false);
    std::string GetSigmaMintFee(
        const vector<CRecipient>& vecSend,
        const vector<sigma::PrivateCoin>& privCoins,
        vector<CHDMint> vDMints,
        CWalletTx &wtxNew,
        int64_t& nFeeRequired,
        const CCoinControl *coinControl = NULL);

    std::string MintAndStoreSigma(
        const vector<CRecipient>& vecSend,
        const vector<sigma::PrivateCoin>& privCoins,
        vector<CHDMint> vDMints,
        CWalletTx &wtxNew,
        bool fAskFee=false,
        const CCoinControl *coinControl = NULL);

    std::string SpendZerocoin(std::string& thirdPartyaddress, int64_t nValue, libzerocoin::CoinDenomination denomination, CWalletTx& wtxNew, CBigNum& coinSerial, uint256& txHash, CBigNum& zcSelectedValue, bool& zcSelectedIsUsed, bool forceUsed = false);

    std::string SpendSigma(std::string& thirdPartyaddress, sigma::CoinDenomination denomination, CWalletTx& wtxNew, Scalar& coinSerial, uint256& txHash, GroupElement& zcSelectedValue, bool& zcSelectedIsUsed, bool forceUsed = false, bool fAskFee=false);

    std::string SpendMultipleZerocoin(std::string& thirdPartyaddress, const std::vector<std::pair<int64_t, libzerocoin::CoinDenomination>>& denominations, CWalletTx& wtxNew, vector<CBigNum>& coinSerials, uint256& txHash, vector<CBigNum>& zcSelectedValues, bool forceUsed = false);

    std::string SpendMultipleSigma(std::string& thirdPartyaddress, const std::vector<sigma::CoinDenomination>& denominations, CWalletTx& wtxNew, vector<Scalar>& coinSerials, uint256& txHash, vector<GroupElement>& zcSelectedValues, bool forceUsed = false, bool fAskFee=false);

    std::vector<CSigmaEntry> SpendSigma(const std::vector<CRecipient>& recipients, CWalletTx& result);
    std::vector<CSigmaEntry> SpendSigma(const std::vector<CRecipient>& recipients, CWalletTx& result, CAmount& fee);

    bool GetMint(const uint256& hashSerial, CSigmaEntry& zerocoin) const;

    bool CreateZerocoinMintModel(string &stringError,
                                 const std::vector<std::pair<std::string,int>>& denominationPairs,
                                 vector<CHDMint>& vDMints,
                                 MintAlgorithm algo);

    bool CreateZerocoinMintModelV2(string &stringError, const string& denomAmount);
    bool CreateSigmaMintModel(string &stringError, const string& denomAmount);

    bool CreateZerocoinMintModel(string &stringError,
                                 const string& denomAmount,
                                 MintAlgorithm algo = ZEROCOIN);

    bool CreateZerocoinMintModel(
        string &stringError,
        const std::vector<std::pair<std::string, int>>& denominationPairs,
        MintAlgorithm algo = ZEROCOIN);

    bool CreateSigmaMintModel(
        string &stringError,
        const std::vector<std::pair<sigma::CoinDenomination, int>>& denominationPairs,
        vector<CHDMint>& vDMints);

    bool CreateZerocoinMintModelV2(string &stringError, const std::vector<std::pair<int,int>>& denominationPairs);

    // If dontSpendSigma is set, spends only old index mints if any. Used in old unit tests.
    bool CreateZerocoinSpendModel(string &stringError, string thirdPartyAddress, string denomAmount, bool forceUsed = false, bool dontSpendSigma = false);

    bool CreateSigmaSpendModel(string &stringError, string thirdPartyAddress, string denomAmount, bool forceUsed = false);
    bool CreateZerocoinSpendModel(CWalletTx& wtx, string &stringError, string& thirdPartyAddress, const vector<string>& denomAmounts, bool forceUsed = false);
    bool CreateZerocoinSpendModelV2(CWalletTx& wtx, string &stringError, string& thirdPartyAddress, const vector<string>& denomAmounts, bool forceUsed = false);
    bool CreateSigmaSpendModel(CWalletTx& wtx, string &stringError, string& thirdPartyAddress, const vector<string>& denomAmounts, bool forceUsed = false);

    //function for spending all old mints form v2 protocol
    bool SpendOldMints(string& stringError);

    bool SetZerocoinBook(const CZerocoinEntry& zerocoinEntry);

    void CommitTransaction(CWalletTx& tx);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);

    bool CreateCollateralTransaction(CMutableTransaction& txCollateral, std::string& strReason);
    bool ConvertList(std::vector<CTxIn> vecTxIn, std::vector<CAmount>& vecAmounts);

    bool AddAccountingEntry(const CAccountingEntry&, CWalletDB& pwalletdb);

    bool CheckDenomination(string denomAmount, int64_t& nAmount, libzerocoin::CoinDenomination& denomination);

    bool CheckHasV2Mint(libzerocoin::CoinDenomination denomination, bool forceUsed);

    // functions to do reminting from zerocoin to sigma
    int GetNumberOfUnspentMintsForDenomination(int version, libzerocoin::CoinDenomination d, CZerocoinEntry *mintEntry = NULL);
    bool CreateZerocoinToSigmaRemintModel(string &stringError, int version, libzerocoin::CoinDenomination d, CWalletTx *wtx = NULL);

    static CFeeRate minTxFee;
    static CFeeRate fallbackFee;
    /**
     * Estimate the minimum fee considering user set parameters
     * and the required fee
     */
    static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool);
    /**
     * Return the minimum required fee taking into account the
     * floating relay fee and user set minimum transaction fee
     */
    static CAmount GetRequiredFee(unsigned int nTxBytes);

    bool NewKeyPool();
    bool TopUpKeyPool(unsigned int kpSize = 0);
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex);
    bool GetKeyFromPool(CPubKey &key);
    int64_t GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const;

    std::set< std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, CAmount> GetAddressBalances();

    CAmount GetAccountBalance(const std::string& strAccount, int nMinDepth, const isminefilter& filter);
    CAmount GetAccountBalance(CWalletDB& walletdb, const std::string& strAccount, int nMinDepth, const isminefilter& filter);
    std::set<CTxDestination> GetAccountAddresses(const std::string& strAccount) const;

    isminetype IsMine(const CTxIn& txin) const;
    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const;
    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool IsChange(const uint256& tx, const CTxOut& txout) const;
    CAmount GetChange(const uint256& tx, const CTxOut& txout) const;
    bool IsMine(const CTransaction& tx) const;
    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction& tx) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetChange(const CTransaction& tx) const;
    void SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut);

    // Remove all CSigmaEntry and CHDMint objects from WalletDB.
    DBErrors ZapSigmaMints();

    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose);

    bool DelAddressBook(const CTxDestination& address);

    void UpdatedTransaction(const uint256 &hashTx);

    void Inventory(const uint256 &hash)
    {
        LOCK(cs_wallet);
        std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
        if (mi != mapRequestCount.end())
            (*mi).second++;
    }

    void GetScriptForMining(boost::shared_ptr<CReserveScript> &script);
    void ResetRequestCount(const uint256 &hash)
    {
        LOCK(cs_wallet);
        mapRequestCount[hash] = 0;
    };

    unsigned int GetKeyPoolSize()
    {
        AssertLockHeld(cs_wallet); // setKeyPool
        return setKeyPool.size();
    }

    bool SetDefaultKey(const CPubKey &vchPubKey);

    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }
    void DisableTransaction(const CTransaction &tx);

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    //! Flush wallet (bitdb flush)
    void Flush(bool shutdown=false);

    //! Verify the wallet database and perform salvage if required
    static bool Verify();

    /**
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination
            &address, const std::string &label, bool isMine,
            const std::string &purpose,
            ChangeType status)> NotifyAddressBookChanged;

    /**
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx,
            ChangeType status)> NotifyTransactionChanged;
    /**
     * Zerocoin entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const std::string &pubCoin, const std::string &isUsed, ChangeType status)> NotifyZerocoinChanged;


    /** Show progress e.g. for rescan */
    boost::signals2::signal<void (const std::string &title, int nProgress)> ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /** Inquire whether this wallet broadcasts transactions. */
    bool GetBroadcastTransactions() const { return fBroadcastTransactions; }
    /** Set whether this wallet broadcasts transactions. */
    void SetBroadcastTransactions(bool broadcast) { fBroadcastTransactions = broadcast; }

    /* Mark a transaction (and it in-wallet descendants) as abandoned so its inputs may be respent. */
    bool AbandonTransaction(const uint256& hashTx);

	/* Staking */
    bool CreateCoinStake(const CKeyStore& keystore, unsigned int nBits, int64_t nTime, int64_t nSearchInterval, CAmount& nFees, CMutableTransaction& tx, CKey& key, CBlockTemplate *pblocktemplate);
    bool SelectCoinsForStaking(CAmount& nTargetValue, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet) const;
    void AvailableCoinsForStaking(std::vector<COutput>& vCoins) const;
    bool HaveAvailableCoinsForStaking() const;
    uint64_t GetStakeWeight() const;

    /* Returns the wallets help message */
    static std::string GetWalletHelpString(bool showDebug);

    /* Initializes the wallet, returns a new CWallet instance or a null pointer in case of an error */
    static bool InitLoadWallet();

    /* Wallets parameter interaction */
    static bool ParameterInteraction();

    bool BackupWallet(const std::string& strDest);

    /* Set the HD chain model (chain child index counters) */
    bool SetHDChain(const CHDChain& chain, bool memonly, bool& upgradeChain = DEFAULT_UPGRADE_CHAIN, bool genNewKeyPool = true);
    const CHDChain& GetHDChain() { return hdChain; }

    bool SetMnemonicContainer(const MnemonicContainer& mnContainer, bool memonly);
    const MnemonicContainer& GetMnemonicContainer() { return mnemonicContainer; }

    bool EncryptMnemonicContainer(const CKeyingMaterial& vMasterKeyIn);
    bool DecryptMnemonicContainer(MnemonicContainer& mnContainer);

    /* Generates a new HD master key (will not be activated) */
    CPubKey GenerateNewHDMasterKey();
    void GenerateNewMnemonic();

    /* Set the current HD master key (will reset the chain child index counters) */
    bool SetHDMasterKey(const CPubKey& key, const int cHDChainVersion=CHDChain().CURRENT_VERSION);
};

/** A key allocated from the key pool. */
class CReserveKey : public CReserveScript
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    CPubKey vchPubKey;
public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey);
    void KeepKey();
    void KeepScript() { KeepKey(); }
};


/**
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};

bool CompHeight(const CZerocoinEntry & a, const CZerocoinEntry & b);
bool CompSigmaHeight(const CSigmaEntry& a, const CSigmaEntry& b);
bool CompID(const CZerocoinEntry & a, const CZerocoinEntry & b);
bool CompSigmaID(const CSigmaEntry& a, const CSigmaEntry& b);
#endif // BITCOIN_WALLET_WALLET_H
