// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FIVEGNODEMAN_H
#define FIVEGNODEMAN_H

#include "fivegnode.h"
#include "sync.h"

using namespace std;

class CFivegnodeMan;

extern CFivegnodeMan mnodeman;

/**
 * Provides a forward and reverse index between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the index exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this index is provided via delegation by CFivegnodeMan
 */
class CFivegnodeIndex
{
public: // Types
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

    typedef std::map<int,CTxIn> rindex_m_t;

    typedef rindex_m_t::iterator rindex_m_it;

    typedef rindex_m_t::const_iterator rindex_m_cit;

private:
    int                  nSize;

    index_m_t            mapIndex;

    rindex_m_t           mapReverseIndex;

public:
    CFivegnodeIndex();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve fivegnode vin by index
    bool Get(int nIndex, CTxIn& vinFivegnode) const;

    /// Get index of a fivegnode vin
    int GetFivegnodeIndex(const CTxIn& vinFivegnode) const;

    void AddFivegnodeVIN(const CTxIn& vinFivegnode);

    void Clear();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapIndex);
        if(ser_action.ForRead()) {
            RebuildIndex();
        }
    }

private:
    void RebuildIndex();

};

class CFivegnodeMan
{
public:
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

private:
    static const int MAX_EXPECTED_INDEX_SIZE = 30000;

    /// Only allow 1 index rebuild per hour
    static const int64_t MIN_INDEX_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 70203;
    static const int MAX_POSE_CONNECTIONS       = 10;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    // map to hold all MNs
    std::vector<CFivegnode> vFivegnodes;
    // who's asked for the Fivegnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForFivegnodeList;
    // who we asked for the Fivegnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForFivegnodeList;
    // which Fivegnodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForFivegnodeListEntry;
    // who we asked for the fivegnode verification
    std::map<CNetAddr, CFivegnodeVerification> mWeAskedForVerification;

    // these maps are used for fivegnode recovery from FIVEGNODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CFivegnodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastIndexRebuildTime;

    CFivegnodeIndex indexFivegnodes;

    CFivegnodeIndex indexFivegnodesOld;

    /// Set when index has been rebuilt, clear when read
    bool fIndexRebuilt;

    /// Set when fivegnodes are added, cleared when CGovernanceManager is notified
    bool fFivegnodesAdded;

    /// Set when fivegnodes are removed, cleared when CGovernanceManager is notified
    bool fFivegnodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastWatchdogVoteTime;

    friend class CFivegnodeSync;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CFivegnodeBroadcast> > mapSeenFivegnodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CFivegnodePing> mapSeenFivegnodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CFivegnodeVerification> mapSeenFivegnodeVerification;
    // keep track of dsq count to prevent fivegnodes from gaming darksend queue
    int64_t nDsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING; 
            READWRITE(strVersion);
        }

        READWRITE(vFivegnodes);
        READWRITE(mAskedUsForFivegnodeList);
        READWRITE(mWeAskedForFivegnodeList);
        READWRITE(mWeAskedForFivegnodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenFivegnodeBroadcast);
        READWRITE(mapSeenFivegnodePing);
        READWRITE(indexFivegnodes);
        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CFivegnodeMan();

    /// Add an entry
    bool Add(CFivegnode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

    /// Check all Fivegnodes
    void Check();

    /// Check all Fivegnodes and remove inactive
    void CheckAndRemove();

    /// Clear Fivegnode vector
    void Clear();

    /// Count Fivegnodes filtered by nProtocolVersion.
    /// Fivegnode nProtocolVersion should match or be above the one specified in param here.
    int CountFivegnodes(int nProtocolVersion = -1);
    /// Count enabled Fivegnodes filtered by nProtocolVersion.
    /// Fivegnode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Fivegnodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CFivegnode* Find(const std::string &txHash, const std::string outputIndex);
    CFivegnode* Find(const CScript &payee);
    CFivegnode* Find(const CTxIn& vin);
    CFivegnode* Find(const CPubKey& pubKeyFivegnode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeyFivegnode, CFivegnode& fivegnode);
    bool Get(const CTxIn& vin, CFivegnode& fivegnode);

    /// Retrieve fivegnode vin by index
    bool Get(int nIndex, CTxIn& vinFivegnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexFivegnodes.Get(nIndex, vinFivegnode);
    }

    bool GetIndexRebuiltFlag() {
        LOCK(cs);
        return fIndexRebuilt;
    }

    /// Get index of a fivegnode vin
    int GetFivegnodeIndex(const CTxIn& vinFivegnode) {
        LOCK(cs);
        return indexFivegnodes.GetFivegnodeIndex(vinFivegnode);
    }

    /// Get old index of a fivegnode vin
    int GetFivegnodeIndexOld(const CTxIn& vinFivegnode) {
        LOCK(cs);
        return indexFivegnodesOld.GetFivegnodeIndex(vinFivegnode);
    }

    /// Get fivegnode VIN for an old index value
    bool GetFivegnodeVinForIndexOld(int nFivegnodeIndex, CTxIn& vinFivegnodeOut) {
        LOCK(cs);
        return indexFivegnodesOld.Get(nFivegnodeIndex, vinFivegnodeOut);
    }

    /// Get index of a fivegnode vin, returning rebuild flag
    int GetFivegnodeIndex(const CTxIn& vinFivegnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexFivegnodes.GetFivegnodeIndex(vinFivegnode);
    }

    void ClearOldFivegnodeIndex() {
        LOCK(cs);
        indexFivegnodesOld.Clear();
        fIndexRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    fivegnode_info_t GetFivegnodeInfo(const CTxIn& vin);

    fivegnode_info_t GetFivegnodeInfo(const CPubKey& pubKeyFivegnode);

    char* GetNotQualifyReason(CFivegnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    UniValue GetNotQualifyReasonToUniValue(CFivegnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    /// Find an entry in the fivegnode list that is next to be paid
    CFivegnode* GetNextFivegnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);
    /// Same as above but use current block height
    CFivegnode* GetNextFivegnodeInQueueForPayment(bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CFivegnode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CFivegnode> GetFullFivegnodeVector() { LOCK(cs); return vFivegnodes; }

    std::vector<std::pair<int, CFivegnode> > GetFivegnodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetFivegnodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CFivegnode* GetFivegnodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessFivegnodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CFivegnode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CFivegnodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CFivegnodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CFivegnodeVerification& mnv);

    /// Return the number of (unique) Fivegnodes
    int size() { return vFivegnodes.size(); }

    std::string ToString() const;

    /// Update fivegnode list and maps using provided CFivegnodeBroadcast
    void UpdateFivegnodeList(CFivegnodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateFivegnodeList(CNode* pfrom, CFivegnodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid();

    void CheckAndRebuildFivegnodeIndex();

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;;
    }

    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);
    bool AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckFivegnode(const CTxIn& vin, bool fForce = false);
    void CheckFivegnode(const CPubKey& pubKeyFivegnode, bool fForce = false);

    int GetFivegnodeState(const CTxIn& vin);
    int GetFivegnodeState(const CPubKey& pubKeyFivegnode);

    bool IsFivegnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetFivegnodeLastPing(const CTxIn& vin, const CFivegnodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex *pindex);

    /**
     * Called to notify CGovernanceManager that the fivegnode index has been updated.
     * Must be called while not holding the CFivegnodeMan::cs mutex
     */
    void NotifyFivegnodeUpdates();

};

#endif
