// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activefivegnode.h"
#include "addrman.h"
#include "darksend.h"
//#include "governance.h"
#include "fivegnode-payments.h"
#include "fivegnode-sync.h"
#include "fivegnode.h"
#include "fivegnodeconfig.h"
#include "fivegnodeman.h"
#include "netfulfilledman.h"
#include "util.h"
#include "validationinterface.h"

/** Fivegnode manager */
CFivegnodeMan mnodeman;

const std::string CFivegnodeMan::SERIALIZATION_VERSION_STRING = "CFivegnodeMan-Version-4";

struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CFivegnode*>& t1,
                    const std::pair<int, CFivegnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CFivegnode*>& t1,
                    const std::pair<int64_t, CFivegnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CFivegnodeIndex::CFivegnodeIndex()
    : nSize(0),
      mapIndex(),
      mapReverseIndex()
{}

bool CFivegnodeIndex::Get(int nIndex, CTxIn& vinFivegnode) const
{
    rindex_m_cit it = mapReverseIndex.find(nIndex);
    if(it == mapReverseIndex.end()) {
        return false;
    }
    vinFivegnode = it->second;
    return true;
}

int CFivegnodeIndex::GetFivegnodeIndex(const CTxIn& vinFivegnode) const
{
    index_m_cit it = mapIndex.find(vinFivegnode);
    if(it == mapIndex.end()) {
        return -1;
    }
    return it->second;
}

void CFivegnodeIndex::AddFivegnodeVIN(const CTxIn& vinFivegnode)
{
    index_m_it it = mapIndex.find(vinFivegnode);
    if(it != mapIndex.end()) {
        return;
    }
    int nNextIndex = nSize;
    mapIndex[vinFivegnode] = nNextIndex;
    mapReverseIndex[nNextIndex] = vinFivegnode;
    ++nSize;
}

void CFivegnodeIndex::Clear()
{
    mapIndex.clear();
    mapReverseIndex.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CFivegnode* t1,
                    const CFivegnode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CFivegnodeIndex::RebuildIndex()
{
    nSize = mapIndex.size();
    for(index_m_it it = mapIndex.begin(); it != mapIndex.end(); ++it) {
        mapReverseIndex[it->second] = it->first;
    }
}

CFivegnodeMan::CFivegnodeMan() : cs(),
  vFivegnodes(),
  mAskedUsForFivegnodeList(),
  mWeAskedForFivegnodeList(),
  mWeAskedForFivegnodeListEntry(),
  mWeAskedForVerification(),
  mMnbRecoveryRequests(),
  mMnbRecoveryGoodReplies(),
  listScheduledMnbRequestConnections(),
  nLastIndexRebuildTime(0),
  indexFivegnodes(),
  indexFivegnodesOld(),
  fIndexRebuilt(false),
  fFivegnodesAdded(false),
  fFivegnodesRemoved(false),
//  vecDirtyGovernanceObjectHashes(),
  nLastWatchdogVoteTime(0),
  mapSeenFivegnodeBroadcast(),
  mapSeenFivegnodePing(),
  nDsqCount(0)
{}

bool CFivegnodeMan::Add(CFivegnode &mn)
{
    LOCK(cs);

    CFivegnode *pmn = Find(mn.vin);
    if (pmn == NULL) {
        LogPrint("fivegnode", "CFivegnodeMan::Add -- Adding new Fivegnode: addr=%s, %i now\n", mn.addr.ToString(), size() + 1);
        vFivegnodes.push_back(mn);
        indexFivegnodes.AddFivegnodeVIN(mn.vin);
        fFivegnodesAdded = true;
        return true;
    }

    return false;
}

void CFivegnodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForFivegnodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForFivegnodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            LogPrintf("CFivegnodeMan::AskForMN -- Asking same peer %s for missing fivegnode entry again: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        } else {
            // we already asked for this outpoint but not this node
            LogPrintf("CFivegnodeMan::AskForMN -- Asking new peer %s for missing fivegnode entry: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        }
    } else {
        // we never asked any node for this outpoint
        LogPrintf("CFivegnodeMan::AskForMN -- Asking peer %s for missing fivegnode entry for the first time: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
    }
    mWeAskedForFivegnodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    pnode->PushMessage(NetMsgType::DSEG, vin);
}

void CFivegnodeMan::Check()
{
    LOCK(cs);

//    LogPrint("fivegnode", "CFivegnodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
        mn.Check();
    }
}

void CFivegnodeMan::CheckAndRemove()
{
    if(!fivegnodeSync.IsFivegnodeListSynced()) return;

    LogPrintf("CFivegnodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdateFivegnodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent fivegnodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CFivegnode>::iterator it = vFivegnodes.begin();
        std::vector<std::pair<int, CFivegnode> > vecFivegnodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES fivegnode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vFivegnodes.end()) {
            CFivegnodeBroadcast mnb = CFivegnodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent()) {
                LogPrint("fivegnode", "CFivegnodeMan::CheckAndRemove -- Removing Fivegnode: %s  addr=%s  %i now\n", (*it).GetStateString(), (*it).addr.ToString(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenFivegnodeBroadcast.erase(hash);
                mWeAskedForFivegnodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
//                it->FlagGovernanceItemsAsDirty();
                it = vFivegnodes.erase(it);
                fFivegnodesRemoved = true;
            } else {
                bool fAsk = pCurrentBlockIndex &&
                            (nAskForMnbRecovery > 0) &&
                            fivegnodeSync.IsSynced() &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk) {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecFivegnodeRanks.empty()) {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockIndex->nHeight);
                        vecFivegnodeRanks = GetFivegnodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL fivegnodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecFivegnodeRanks.size(); i++) {
                        // avoid banning
                        if(mWeAskedForFivegnodeListEntry.count(it->vin.prevout) && mWeAskedForFivegnodeListEntry[it->vin.prevout].count(vecFivegnodeRanks[i].second.addr)) continue;
                        // didn't ask recently, ok to ask now
                        CService addr = vecFivegnodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery) {
                        LogPrint("fivegnode", "CFivegnodeMan::CheckAndRemove -- Recovery initiated, fivegnode=%s\n", it->vin.prevout.ToStringShort());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for FIVEGNODE_NEW_START_REQUIRED fivegnodes
        LogPrint("fivegnode", "CFivegnodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CFivegnodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end()){
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime()) {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED) {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    LogPrint("fivegnode", "CFivegnodeMan::CheckAndRemove -- reprocessing mnb, fivegnode=%s\n", itMnbReplies->second[0].vin.prevout.ToStringShort());
                    // mapSeenFivegnodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdateFivegnodeList(NULL, itMnbReplies->second[0], nDos);
                }
                LogPrint("fivegnode", "CFivegnodeMan::CheckAndRemove -- removing mnb recovery reply, fivegnode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToStringShort(), (int)itMnbReplies->second.size());
                mMnbRecoveryGoodReplies.erase(itMnbReplies++);
            } else {
                ++itMnbReplies;
            }
        }
    }
    {
        // no need for cm_main below
        LOCK(cs);

        std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > >::iterator itMnbRequest = mMnbRecoveryRequests.begin();
        while(itMnbRequest != mMnbRecoveryRequests.end()){
            // Allow this mnb to be re-verified again after MNB_RECOVERY_RETRY_SECONDS seconds
            // if mn is still in FIVEGNODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS) {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            } else {
                ++itMnbRequest;
            }
        }

        // check who's asked for the Fivegnode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForFivegnodeList.begin();
        while(it1 != mAskedUsForFivegnodeList.end()){
            if((*it1).second < GetTime()) {
                mAskedUsForFivegnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check who we asked for the Fivegnode list
        it1 = mWeAskedForFivegnodeList.begin();
        while(it1 != mWeAskedForFivegnodeList.end()){
            if((*it1).second < GetTime()){
                mWeAskedForFivegnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check which Fivegnodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForFivegnodeListEntry.begin();
        while(it2 != mWeAskedForFivegnodeListEntry.end()){
            std::map<CNetAddr, int64_t>::iterator it3 = it2->second.begin();
            while(it3 != it2->second.end()){
                if(it3->second < GetTime()){
                    it2->second.erase(it3++);
                } else {
                    ++it3;
                }
            }
            if(it2->second.empty()) {
                mWeAskedForFivegnodeListEntry.erase(it2++);
            } else {
                ++it2;
            }
        }

        std::map<CNetAddr, CFivegnodeVerification>::iterator it3 = mWeAskedForVerification.begin();
        while(it3 != mWeAskedForVerification.end()){
            if(it3->second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
                mWeAskedForVerification.erase(it3++);
            } else {
                ++it3;
            }
        }

        // NOTE: do not expire mapSeenFivegnodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenFivegnodePing
        std::map<uint256, CFivegnodePing>::iterator it4 = mapSeenFivegnodePing.begin();
        while(it4 != mapSeenFivegnodePing.end()){
            if((*it4).second.IsExpired()) {
                LogPrint("fivegnode", "CFivegnodeMan::CheckAndRemove -- Removing expired Fivegnode ping: hash=%s\n", (*it4).second.GetHash().ToString());
                mapSeenFivegnodePing.erase(it4++);
            } else {
                ++it4;
            }
        }

        // remove expired mapSeenFivegnodeVerification
        std::map<uint256, CFivegnodeVerification>::iterator itv2 = mapSeenFivegnodeVerification.begin();
        while(itv2 != mapSeenFivegnodeVerification.end()){
            if((*itv2).second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS){
                LogPrint("fivegnode", "CFivegnodeMan::CheckAndRemove -- Removing expired Fivegnode verification: hash=%s\n", (*itv2).first.ToString());
                mapSeenFivegnodeVerification.erase(itv2++);
            } else {
                ++itv2;
            }
        }

        LogPrintf("CFivegnodeMan::CheckAndRemove -- %s\n", ToString());

        if(fFivegnodesRemoved) {
            CheckAndRebuildFivegnodeIndex();
        }
    }

    if(fFivegnodesRemoved) {
        NotifyFivegnodeUpdates();
    }
}

void CFivegnodeMan::Clear()
{
    LOCK(cs);
    vFivegnodes.clear();
    mAskedUsForFivegnodeList.clear();
    mWeAskedForFivegnodeList.clear();
    mWeAskedForFivegnodeListEntry.clear();
    mapSeenFivegnodeBroadcast.clear();
    mapSeenFivegnodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    indexFivegnodes.Clear();
    indexFivegnodesOld.Clear();
}

int CFivegnodeMan::CountFivegnodes(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinFivegnodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
        if(mn.nProtocolVersion < nProtocolVersion) continue;
        nCount++;
    }

    return nCount;
}

int CFivegnodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinFivegnodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
        if(mn.nProtocolVersion < nProtocolVersion || !mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 fivegnodes are allowed in 12.1, saving this for later
int CFivegnodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CFivegnodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForFivegnodeList.find(pnode->addr);
            if(it != mWeAskedForFivegnodeList.end() && GetTime() < (*it).second) {
                LogPrintf("CFivegnodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString());
                return;
            }
        }
    }
    
    pnode->PushMessage(NetMsgType::DSEG, CTxIn());
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForFivegnodeList[pnode->addr] = askAgain;

    LogPrint("fivegnode", "CFivegnodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString());
}

CFivegnode* CFivegnodeMan::Find(const std::string &txHash, const std::string outputIndex)
{
    LOCK(cs);

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes)
    {
        COutPoint outpoint = mn.vin.prevout;

        if(txHash==outpoint.hash.ToString().substr(0,64) &&
           outputIndex==to_string(outpoint.n))
            return &mn;
    }
    return NULL;
}

CFivegnode* CFivegnodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes)
    {
        if(GetScriptForDestination(mn.pubKeyCollateralAddress.GetID()) == payee)
            return &mn;
    }
    return NULL;
}

CFivegnode* CFivegnodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes)
    {
        if(mn.vin.prevout == vin.prevout)
            return &mn;
    }
    return NULL;
}

CFivegnode* CFivegnodeMan::Find(const CPubKey &pubKeyFivegnode)
{
    LOCK(cs);

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes)
    {
        if(mn.pubKeyFivegnode == pubKeyFivegnode)
            return &mn;
    }
    return NULL;
}

bool CFivegnodeMan::Get(const CPubKey& pubKeyFivegnode, CFivegnode& fivegnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CFivegnode* pMN = Find(pubKeyFivegnode);
    if(!pMN)  {
        return false;
    }
    fivegnode = *pMN;
    return true;
}

bool CFivegnodeMan::Get(const CTxIn& vin, CFivegnode& fivegnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    fivegnode = *pMN;
    return true;
}

fivegnode_info_t CFivegnodeMan::GetFivegnodeInfo(const CTxIn& vin)
{
    fivegnode_info_t info;
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

fivegnode_info_t CFivegnodeMan::GetFivegnodeInfo(const CPubKey& pubKeyFivegnode)
{
    fivegnode_info_t info;
    LOCK(cs);
    CFivegnode* pMN = Find(pubKeyFivegnode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CFivegnodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    return (pMN != NULL);
}

char* CFivegnodeMan::GetNotQualifyReason(CFivegnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    if (!mn.IsValidForPayment()) {
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'not valid for payment'");
        return reasonStr;
    }
    // //check protocol version
    if (mn.nProtocolVersion < mnpayments.GetMinFivegnodePaymentsProto()) {
        // LogPrintf("Invalid nProtocolVersion!\n");
        // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
        // LogPrintf("mnpayments.GetMinFivegnodePaymentsProto=%s!\n", mnpayments.GetMinFivegnodePaymentsProto());
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'Invalid nProtocolVersion', nProtocolVersion=%d", mn.nProtocolVersion);
        return reasonStr;
    }
    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        // LogPrintf("mnpayments.IsScheduled!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'is scheduled'");
        return reasonStr;
    }
    //it's too new, wait for a cycle
    if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // LogPrintf("it's too new, wait for a cycle!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'too new', sigTime=%s, will be qualifed after=%s",
                DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
        return reasonStr;
    }
    //make sure it has at least as many confirmations as there are fivegnodes
    if (mn.GetCollateralAge() < nMnCount) {
        // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
        // LogPrintf("nMnCount=%s!\n", nMnCount);
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'collateralAge < znCount', collateralAge=%d, znCount=%d", mn.GetCollateralAge(), nMnCount);
        return reasonStr;
    }
    return NULL;
}

// Same method, different return type, to avoid Fivegnode operator issues.
// TODO: discuss standardizing the JSON type here, as it's done everywhere else in the code.
UniValue CFivegnodeMan::GetNotQualifyReasonToUniValue(CFivegnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    UniValue ret(UniValue::VOBJ);
    UniValue data(UniValue::VOBJ);
    string description;

    if (!mn.IsValidForPayment()) {
        description = "not valid for payment";
    }

    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    else if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        description = "Is scheduled";
    }

    // //check protocol version
    else if (mn.nProtocolVersion < mnpayments.GetMinFivegnodePaymentsProto()) {
        description = "Invalid nProtocolVersion";

        data.push_back(Pair("nProtocolVersion", mn.nProtocolVersion));
    }

    //it's too new, wait for a cycle
    else if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // LogPrintf("it's too new, wait for a cycle!\n");
        description = "Too new";

        //TODO unix timestamp
        data.push_back(Pair("sigTime", mn.sigTime));
        data.push_back(Pair("qualifiedAfter", mn.sigTime + (nMnCount * 2.6 * 60)));
    }
    //make sure it has at least as many confirmations as there are fivegnodes
    else if (mn.GetCollateralAge() < nMnCount) {
        description = "collateralAge < znCount";

        data.push_back(Pair("collateralAge", mn.GetCollateralAge()));
        data.push_back(Pair("znCount", nMnCount));
    }

    ret.push_back(Pair("result", description.empty()));
    if(!description.empty()){
        ret.push_back(Pair("description", description));
    }
    if(!data.empty()){
        ret.push_back(Pair("data", data));
    }

    return ret;
}

//
// Deterministically select the oldest/best fivegnode to pay on the network
//
CFivegnode* CFivegnodeMan::GetNextFivegnodeInQueueForPayment(bool fFilterSigTime, int& nCount)
{
    if(!pCurrentBlockIndex) {
        nCount = 0;
        return NULL;
    }
    return GetNextFivegnodeInQueueForPayment(pCurrentBlockIndex->nHeight, fFilterSigTime, nCount);
}

CFivegnode* CFivegnodeMan::GetNextFivegnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    // Need LOCK2 here to ensure consistent locking order because the GetBlockHash call below locks cs_main
    LOCK2(cs_main,cs);

    CFivegnode *pBestFivegnode = NULL;
    std::vector<std::pair<int, CFivegnode*> > vecFivegnodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */
    int nMnCount = CountEnabled();
    int index = 0;
    BOOST_FOREACH(CFivegnode &mn, vFivegnodes)
    {
        index += 1;
        // LogPrintf("fiveg=%s, mn=%s\n", index, mn.ToString());
        /*if (!mn.IsValidForPayment()) {
            LogPrint("fivegnodeman", "Fivegnode, %s, addr(%s), not-qualified: 'not valid for payment'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        // //check protocol version
        if (mn.nProtocolVersion < mnpayments.GetMinFivegnodePaymentsProto()) {
            // LogPrintf("Invalid nProtocolVersion!\n");
            // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
            // LogPrintf("mnpayments.GetMinFivegnodePaymentsProto=%s!\n", mnpayments.GetMinFivegnodePaymentsProto());
            LogPrint("fivegnodeman", "Fivegnode, %s, addr(%s), not-qualified: 'invalid nProtocolVersion'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
        if (mnpayments.IsScheduled(mn, nBlockHeight)) {
            // LogPrintf("mnpayments.IsScheduled!\n");
            LogPrint("fivegnodeman", "Fivegnode, %s, addr(%s), not-qualified: 'IsScheduled'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's too new, wait for a cycle
        if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
            // LogPrintf("it's too new, wait for a cycle!\n");
            LogPrint("fivegnodeman", "Fivegnode, %s, addr(%s), not-qualified: 'it's too new, wait for a cycle!', sigTime=%s, will be qualifed after=%s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
            continue;
        }
        //make sure it has at least as many confirmations as there are fivegnodes
        if (mn.GetCollateralAge() < nMnCount) {
            // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
            // LogPrintf("nMnCount=%s!\n", nMnCount);
            LogPrint("fivegnodeman", "Fivegnode, %s, addr(%s), not-qualified: 'mn.GetCollateralAge() < nMnCount', CollateralAge=%d, nMnCount=%d\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), mn.GetCollateralAge(), nMnCount);
            continue;
        }*/
        char* reasonStr = GetNotQualifyReason(mn, nBlockHeight, fFilterSigTime, nMnCount);
        if (reasonStr != NULL) {
            LogPrint("fivegnodeman", "Fivegnode, %s, addr(%s), qualify %s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), reasonStr);
            delete [] reasonStr;
            continue;
        }
        vecFivegnodeLastPaid.push_back(std::make_pair(mn.GetLastPaidBlock(), &mn));
    }
    nCount = (int)vecFivegnodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if(fFilterSigTime && nCount < nMnCount / 3) {
        // LogPrintf("Need Return, nCount=%s, nMnCount/3=%s\n", nCount, nMnCount/3);
        return GetNextFivegnodeInQueueForPayment(nBlockHeight, false, nCount);
    }

    // Sort them low to high
    sort(vecFivegnodeLastPaid.begin(), vecFivegnodeLastPaid.end(), CompareLastPaidBlock());

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight - 101)) {
        LogPrintf("CFivegnode::GetNextFivegnodeInQueueForPayment -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight - 101);
        return NULL;
    }
    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = nMnCount/10;
    int nCountTenth = 0;
    arith_uint256 nHighest = 0;
    BOOST_FOREACH (PAIRTYPE(int, CFivegnode*)& s, vecFivegnodeLastPaid){
        arith_uint256 nScore = s.second->CalculateScore(blockHash);
        if(nScore > nHighest){
            nHighest = nScore;
            pBestFivegnode = s.second;
        }
        nCountTenth++;
        if(nCountTenth >= nTenthNetwork) break;
    }
    return pBestFivegnode;
}

CFivegnode* CFivegnodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinFivegnodePaymentsProto() : nProtocolVersion;

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    LogPrintf("CFivegnodeMan::FindRandomNotInVec -- %d enabled fivegnodes, %d fivegnodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1) return NULL;

    // fill a vector of pointers
    std::vector<CFivegnode*> vpFivegnodesShuffled;
    BOOST_FOREACH(CFivegnode &mn, vFivegnodes) {
        vpFivegnodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpFivegnodesShuffled.begin(), vpFivegnodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    BOOST_FOREACH(CFivegnode* pmn, vpFivegnodesShuffled) {
        if(pmn->nProtocolVersion < nProtocolVersion || !pmn->IsEnabled()) continue;
        fExclude = false;
        BOOST_FOREACH(const CTxIn &txinToExclude, vecToExclude) {
            if(pmn->vin.prevout == txinToExclude.prevout) {
                fExclude = true;
                break;
            }
        }
        if(fExclude) continue;
        // found the one not in vecToExclude
        LogPrint("fivegnode", "CFivegnodeMan::FindRandomNotInVec -- found, fivegnode=%s\n", pmn->vin.prevout.ToStringShort());
        return pmn;
    }

    LogPrint("fivegnode", "CFivegnodeMan::FindRandomNotInVec -- failed\n");
    return NULL;
}

int CFivegnodeMan::GetFivegnodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CFivegnode*> > vecFivegnodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return -1;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive) {
            if(!mn.IsEnabled()) continue;
        }
        else {
            if(!mn.IsValidForPayment()) continue;
        }
        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecFivegnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecFivegnodeScores.rbegin(), vecFivegnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CFivegnode*)& scorePair, vecFivegnodeScores) {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout) return nRank;
    }

    return -1;
}

std::vector<std::pair<int, CFivegnode> > CFivegnodeMan::GetFivegnodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CFivegnode*> > vecFivegnodeScores;
    std::vector<std::pair<int, CFivegnode> > vecFivegnodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return vecFivegnodeRanks;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {

        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecFivegnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecFivegnodeScores.rbegin(), vecFivegnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CFivegnode*)& s, vecFivegnodeScores) {
        nRank++;
        s.second->SetRank(nRank);
        vecFivegnodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecFivegnodeRanks;
}

CFivegnode* CFivegnodeMan::GetFivegnodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CFivegnode*> > vecFivegnodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        LogPrintf("CFivegnode::GetFivegnodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return NULL;
    }

    // Fill scores
    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {

        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive && !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecFivegnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecFivegnodeScores.rbegin(), vecFivegnodeScores.rend(), CompareScoreMN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CFivegnode*)& s, vecFivegnodeScores){
        rank++;
        if(rank == nRank) {
            return s.second;
        }
    }

    return NULL;
}

void CFivegnodeMan::ProcessFivegnodeConnections()
{
    //we don't care about this for regtest
    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        if(pnode->fFivegnode) {
            if(darkSendPool.pSubmittedToFivegnode != NULL && pnode->addr == darkSendPool.pSubmittedToFivegnode->addr) continue;
            // LogPrintf("Closing Fivegnode connection: peer=%d, addr=%s\n", pnode->id, pnode->addr.ToString());
            pnode->fDisconnect = true;
        }
    }
}

std::pair<CService, std::set<uint256> > CFivegnodeMan::PopScheduledMnbRequestConnection()
{
    LOCK(cs);
    if(listScheduledMnbRequestConnections.empty()) {
        return std::make_pair(CService(), std::set<uint256>());
    }

    std::set<uint256> setResult;

    listScheduledMnbRequestConnections.sort();
    std::pair<CService, uint256> pairFront = listScheduledMnbRequestConnections.front();

    // squash hashes from requests with the same CService as the first one into setResult
    std::list< std::pair<CService, uint256> >::iterator it = listScheduledMnbRequestConnections.begin();
    while(it != listScheduledMnbRequestConnections.end()) {
        if(pairFront.first == it->first) {
            setResult.insert(it->second);
            it = listScheduledMnbRequestConnections.erase(it);
        } else {
            // since list is sorted now, we can be sure that there is no more hashes left
            // to ask for from this addr
            break;
        }
    }
    return std::make_pair(pairFront.first, setResult);
}


void CFivegnodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{

//    LogPrint("fivegnode", "CFivegnodeMan::ProcessMessage, strCommand=%s\n", strCommand);
    if(fLiteMode) return; // disable all Index specific functionality
    if(!fivegnodeSync.IsBlockchainSynced()) return;

    if (strCommand == NetMsgType::MNANNOUNCE) { //Fivegnode Broadcast
        CFivegnodeBroadcast mnb;
        vRecv >> mnb;

        pfrom->setAskFor.erase(mnb.GetHash());

        LogPrintf("MNANNOUNCE -- Fivegnode announce, fivegnode=%s\n", mnb.vin.prevout.ToStringShort());

        int nDos = 0;

        if (CheckMnbAndUpdateFivegnodeList(pfrom, mnb, nDos)) {
            // use announced Fivegnode as a peer
            addrman.Add(CAddress(mnb.addr, NODE_NETWORK), pfrom->addr, 2*60*60);
        } else if(nDos > 0) {
            Misbehaving(pfrom->GetId(), nDos);
        }

        if(fFivegnodesAdded) {
            NotifyFivegnodeUpdates();
        }
    } else if (strCommand == NetMsgType::MNPING) { //Fivegnode Ping

        CFivegnodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        pfrom->setAskFor.erase(nHash);

        LogPrint("fivegnode", "MNPING -- Fivegnode ping, fivegnode=%s\n", mnp.vin.prevout.ToStringShort());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenFivegnodePing.count(nHash)) return; //seen
        mapSeenFivegnodePing.insert(std::make_pair(nHash, mnp));

        LogPrint("fivegnode", "MNPING -- Fivegnode ping, fivegnode=%s new\n", mnp.vin.prevout.ToStringShort());

        // see if we have this Fivegnode
        CFivegnode* pmn = mnodeman.Find(mnp.vin);

        // too late, new MNANNOUNCE is required
        if(pmn && pmn->IsNewStartRequired()) return;

        int nDos = 0;
        if(mnp.CheckAndUpdate(pmn, false, nDos)) return;

        if(nDos > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDos);
        } else if(pmn != NULL) {
            // nothing significant failed, mn is a known one too
            return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a fivegnode entry once
        AskForMN(pfrom, mnp.vin);

    } else if (strCommand == NetMsgType::DSEG) { //Get Fivegnode list or specific entry
        // Ignore such requests until we are fully synced.
        // We could start processing this after fivegnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!fivegnodeSync.IsSynced()) return;

        CTxIn vin;
        vRecv >> vin;

        LogPrint("fivegnode", "DSEG -- Fivegnode list, fivegnode=%s\n", vin.prevout.ToStringShort());

        LOCK(cs);

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && Params().NetworkIDString() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForFivegnodeList.find(pfrom->addr);
                if (i != mAskedUsForFivegnodeList.end()){
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("DSEG -- peer already asked me for the list, peer=%d\n", pfrom->id);
                        return;
                    }
                }
                int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
                mAskedUsForFivegnodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
            if (vin != CTxIn() && vin != mn.vin) continue; // asked for specific vin but we are not there yet
            if (mn.addr.IsRFC1918() || mn.addr.IsLocal()) continue; // do not send local network fivegnode
            if (mn.IsUpdateRequired()) continue; // do not send outdated fivegnodes

            LogPrint("fivegnode", "DSEG -- Sending Fivegnode entry: fivegnode=%s  addr=%s\n", mn.vin.prevout.ToStringShort(), mn.addr.ToString());
            CFivegnodeBroadcast mnb = CFivegnodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_FIVEGNODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_FIVEGNODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenFivegnodeBroadcast.count(hash)) {
                mapSeenFivegnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin) {
                LogPrintf("DSEG -- Sent 1 Fivegnode inv to peer %d\n", pfrom->id);
                return;
            }
        }

        if(vin == CTxIn()) {
            pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, FIVEGNODE_SYNC_LIST, nInvCount);
            LogPrintf("DSEG -- Sent %d Fivegnode invs to peer %d\n", nInvCount, pfrom->id);
            return;
        }
        // smth weird happen - someone asked us for vin we have no idea about?
        LogPrint("fivegnode", "DSEG -- No invs sent to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::MNVERIFY) { // Fivegnode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CFivegnodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty()) {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        } else if (mnv.vchSig2.empty()) {
            // CASE 2: we _probably_ got verification we requested from some fivegnode
            ProcessVerifyReply(pfrom, mnv);
        } else {
            // CASE 3: we _probably_ got verification broadcast signed by some fivegnode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of fivegnodes via unique direct requests.

void CFivegnodeMan::DoFullVerificationStep()
{
    if(activeFivegnode.vin == CTxIn()) return;
    if(!fivegnodeSync.IsSynced()) return;

    std::vector<std::pair<int, CFivegnode> > vecFivegnodeRanks = GetFivegnodeRanks(pCurrentBlockIndex->nHeight - 1, MIN_POSE_PROTO_VERSION);

    // Need LOCK2 here to ensure consistent locking order because the SendVerifyRequest call below locks cs_main
    // through GetHeight() signal in ConnectNode
    LOCK2(cs_main, cs);

    int nCount = 0;

    int nMyRank = -1;
    int nRanksTotal = (int)vecFivegnodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CFivegnode> >::iterator it = vecFivegnodeRanks.begin();
    while(it != vecFivegnodeRanks.end()) {
        if(it->first > MAX_POSE_RANK) {
            LogPrint("fivegnode", "CFivegnodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                        (int)MAX_POSE_RANK);
            return;
        }
        if(it->second.vin == activeFivegnode.vin) {
            nMyRank = it->first;
            LogPrint("fivegnode", "CFivegnodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d fivegnodes\n",
                        nMyRank, nRanksTotal, (int)MAX_POSE_CONNECTIONS);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this fivegnode is not enabled
    if(nMyRank == -1) return;

    // send verify requests to up to MAX_POSE_CONNECTIONS fivegnodes
    // starting from MAX_POSE_RANK + nMyRank and using MAX_POSE_CONNECTIONS as a step
    int nOffset = MAX_POSE_RANK + nMyRank - 1;
    if(nOffset >= (int)vecFivegnodeRanks.size()) return;

    std::vector<CFivegnode*> vSortedByAddr;
    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecFivegnodeRanks.begin() + nOffset;
    while(it != vecFivegnodeRanks.end()) {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned()) {
            LogPrint("fivegnode", "CFivegnodeMan::DoFullVerificationStep -- Already %s%s%s fivegnode %s address %s, skipping...\n",
                        it->second.IsPoSeVerified() ? "verified" : "",
                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
                        it->second.IsPoSeBanned() ? "banned" : "",
                        it->second.vin.prevout.ToStringShort(), it->second.addr.ToString());
            nOffset += MAX_POSE_CONNECTIONS;
            if(nOffset >= (int)vecFivegnodeRanks.size()) break;
            it += MAX_POSE_CONNECTIONS;
            continue;
        }
        LogPrint("fivegnode", "CFivegnodeMan::DoFullVerificationStep -- Verifying fivegnode %s rank %d/%d address %s\n",
                    it->second.vin.prevout.ToStringShort(), it->first, nRanksTotal, it->second.addr.ToString());
        if(SendVerifyRequest(CAddress(it->second.addr, NODE_NETWORK), vSortedByAddr)) {
            nCount++;
            if(nCount >= MAX_POSE_CONNECTIONS) break;
        }
        nOffset += MAX_POSE_CONNECTIONS;
        if(nOffset >= (int)vecFivegnodeRanks.size()) break;
        it += MAX_POSE_CONNECTIONS;
    }

    LogPrint("fivegnode", "CFivegnodeMan::DoFullVerificationStep -- Sent verification requests to %d fivegnodes\n", nCount);
}

// This function tries to find fivegnodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CFivegnodeMan::CheckSameAddr()
{
    if(!fivegnodeSync.IsSynced() || vFivegnodes.empty()) return;

    std::vector<CFivegnode*> vBan;
    std::vector<CFivegnode*> vSortedByAddr;

    {
        LOCK(cs);

        CFivegnode* pprevFivegnode = NULL;
        CFivegnode* pverifiedFivegnode = NULL;

        BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        BOOST_FOREACH(CFivegnode* pmn, vSortedByAddr) {
            // check only (pre)enabled fivegnodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled()) continue;
            // initial step
            if(!pprevFivegnode) {
                pprevFivegnode = pmn;
                pverifiedFivegnode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevFivegnode->addr) {
                if(pverifiedFivegnode) {
                    // another fivegnode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                } else if(pmn->IsPoSeVerified()) {
                    // this fivegnode with the same ip is verified, ban previous one
                    vBan.push_back(pprevFivegnode);
                    // and keep a reference to be able to ban following fivegnodes with the same ip
                    pverifiedFivegnode = pmn;
                }
            } else {
                pverifiedFivegnode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevFivegnode = pmn;
        }
    }

    // ban duplicates
    BOOST_FOREACH(CFivegnode* pmn, vBan) {
        LogPrintf("CFivegnodeMan::CheckSameAddr -- increasing PoSe ban score for fivegnode %s\n", pmn->vin.prevout.ToStringShort());
        pmn->IncreasePoSeBanScore();
    }
}

bool CFivegnodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CFivegnode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        // we already asked for verification, not a good idea to do this too often, skip it
        LogPrint("fivegnode", "CFivegnodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString());
        return false;
    }

    CNode* pnode = ConnectNode(addr, NULL, false, true);
    if(pnode == NULL) {
        LogPrintf("CFivegnodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CFivegnodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockIndex->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    LogPrintf("CFivegnodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString());
    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);

    return true;
}

void CFivegnodeMan::SendVerifyReply(CNode* pnode, CFivegnodeVerification& mnv)
{
    // only fivegnodes can sign this, why would someone ask regular node?
    if(!fFivegNode) {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply")) {
//        // peer should not ask us that often
        LogPrintf("FivegnodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%d\n", pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        LogPrintf("FivegnodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activeFivegnode.service.ToString(), mnv.nonce, blockHash.ToString());

    if(!darkSendSigner.SignMessage(strMessage, mnv.vchSig1, activeFivegnode.keyFivegnode)) {
        LogPrintf("FivegnodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    if(!darkSendSigner.VerifyMessage(activeFivegnode.pubKeyFivegnode, mnv.vchSig1, strMessage, strError)) {
        LogPrintf("FivegnodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
        return;
    }

    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CFivegnodeMan::ProcessVerifyReply(CNode* pnode, CFivegnodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        LogPrintf("CFivegnodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s, peer=%d\n", pnode->addr.ToString(), pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce) {
        LogPrintf("CFivegnodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight) {
        LogPrintf("CFivegnodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("FivegnodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

//    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done")) {
        LogPrintf("CFivegnodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString());
        Misbehaving(pnode->id, 20);
        return;
    }

    {
        LOCK(cs);

        CFivegnode* prealFivegnode = NULL;
        std::vector<CFivegnode*> vpFivegnodesToBan;
        std::vector<CFivegnode>::iterator it = vFivegnodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(), mnv.nonce, blockHash.ToString());
        while(it != vFivegnodes.end()) {
            if(CAddress(it->addr, NODE_NETWORK) == pnode->addr) {
                if(darkSendSigner.VerifyMessage(it->pubKeyFivegnode, mnv.vchSig1, strMessage1, strError)) {
                    // found it!
                    prealFivegnode = &(*it);
                    if(!it->IsPoSeVerified()) {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated fivegnode
                    if(activeFivegnode.vin == CTxIn()) continue;
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activeFivegnode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());
                    // ... and sign it
                    if(!darkSendSigner.SignMessage(strMessage2, mnv.vchSig2, activeFivegnode.keyFivegnode)) {
                        LogPrintf("FivegnodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    if(!darkSendSigner.VerifyMessage(activeFivegnode.pubKeyFivegnode, mnv.vchSig2, strMessage2, strError)) {
                        LogPrintf("FivegnodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                } else {
                    vpFivegnodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real fivegnode found?...
        if(!prealFivegnode) {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            LogPrintf("CFivegnodeMan::ProcessVerifyReply -- ERROR: no real fivegnode found for addr %s\n", pnode->addr.ToString());
            Misbehaving(pnode->id, 20);
            return;
        }
        LogPrintf("CFivegnodeMan::ProcessVerifyReply -- verified real fivegnode %s for addr %s\n",
                    prealFivegnode->vin.prevout.ToStringShort(), pnode->addr.ToString());
        // increase ban score for everyone else
        BOOST_FOREACH(CFivegnode* pmn, vpFivegnodesToBan) {
            pmn->IncreasePoSeBanScore();
            LogPrint("fivegnode", "CFivegnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        prealFivegnode->vin.prevout.ToStringShort(), pnode->addr.ToString(), pmn->nPoSeBanScore);
        }
        LogPrintf("CFivegnodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake fivegnodes, addr %s\n",
                    (int)vpFivegnodesToBan.size(), pnode->addr.ToString());
    }
}

void CFivegnodeMan::ProcessVerifyBroadcast(CNode* pnode, const CFivegnodeVerification& mnv)
{
    std::string strError;

    if(mapSeenFivegnodeVerification.find(mnv.GetHash()) != mapSeenFivegnodeVerification.end()) {
        // we already have one
        return;
    }
    mapSeenFivegnodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
        LogPrint("fivegnode", "FivegnodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%d\n",
                    pCurrentBlockIndex->nHeight, mnv.nBlockHeight, pnode->id);
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout) {
        LogPrint("fivegnode", "FivegnodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%d\n",
                    mnv.vin1.prevout.ToStringShort(), pnode->id);
        // that was NOT a good idea to cheat and verify itself,
        // ban the node we received such message from
        Misbehaving(pnode->id, 100);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("FivegnodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    int nRank = GetFivegnodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);

    if (nRank == -1) {
        LogPrint("fivegnode", "CFivegnodeMan::ProcessVerifyBroadcast -- Can't calculate rank for fivegnode %s\n",
                    mnv.vin2.prevout.ToStringShort());
        return;
    }

    if(nRank > MAX_POSE_RANK) {
        LogPrint("fivegnode", "CFivegnodeMan::ProcessVerifyBroadcast -- Mastrernode %s is not in top %d, current rank %d, peer=%d\n",
                    mnv.vin2.prevout.ToStringShort(), (int)MAX_POSE_RANK, nRank, pnode->id);
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());

        CFivegnode* pmn1 = Find(mnv.vin1);
        if(!pmn1) {
            LogPrintf("CFivegnodeMan::ProcessVerifyBroadcast -- can't find fivegnode1 %s\n", mnv.vin1.prevout.ToStringShort());
            return;
        }

        CFivegnode* pmn2 = Find(mnv.vin2);
        if(!pmn2) {
            LogPrintf("CFivegnodeMan::ProcessVerifyBroadcast -- can't find fivegnode2 %s\n", mnv.vin2.prevout.ToStringShort());
            return;
        }

        if(pmn1->addr != mnv.addr) {
            LogPrintf("CFivegnodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString(), pnode->addr.ToString());
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn1->pubKeyFivegnode, mnv.vchSig1, strMessage1, strError)) {
            LogPrintf("FivegnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for fivegnode1 failed, error: %s\n", strError);
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn2->pubKeyFivegnode, mnv.vchSig2, strMessage2, strError)) {
            LogPrintf("FivegnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for fivegnode2 failed, error: %s\n", strError);
            return;
        }

        if(!pmn1->IsPoSeVerified()) {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        LogPrintf("CFivegnodeMan::ProcessVerifyBroadcast -- verified fivegnode %s for addr %s\n",
                    pmn1->vin.prevout.ToStringShort(), pnode->addr.ToString());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout) continue;
            mn.IncreasePoSeBanScore();
            nCount++;
            LogPrint("fivegnode", "CFivegnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        mn.vin.prevout.ToStringShort(), mn.addr.ToString(), mn.nPoSeBanScore);
        }
        LogPrintf("CFivegnodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake fivegnodes, addr %s\n",
                    nCount, pnode->addr.ToString());
    }
}

std::string CFivegnodeMan::ToString() const
{
    std::ostringstream info;

    info << "Fivegnodes: " << (int)vFivegnodes.size() <<
            ", peers who asked us for Fivegnode list: " << (int)mAskedUsForFivegnodeList.size() <<
            ", peers we asked for Fivegnode list: " << (int)mWeAskedForFivegnodeList.size() <<
            ", entries in Fivegnode list we asked for: " << (int)mWeAskedForFivegnodeListEntry.size() <<
            ", fivegnode index size: " << indexFivegnodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CFivegnodeMan::UpdateFivegnodeList(CFivegnodeBroadcast mnb)
{
    try {
        LogPrintf("CFivegnodeMan::UpdateFivegnodeList\n");
        LOCK2(cs_main, cs);
        mapSeenFivegnodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
        mapSeenFivegnodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

        LogPrintf("CFivegnodeMan::UpdateFivegnodeList -- fivegnode=%s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());

        CFivegnode *pmn = Find(mnb.vin);
        if (pmn == NULL) {
            CFivegnode mn(mnb);
            if (Add(mn)) {
                fivegnodeSync.AddedFivegnodeList();
                GetMainSignals().UpdatedFivegnode(mn);
            }
        } else {
            CFivegnodeBroadcast mnbOld = mapSeenFivegnodeBroadcast[CFivegnodeBroadcast(*pmn).GetHash()].second;
            if (pmn->UpdateFromNewBroadcast(mnb)) {
                fivegnodeSync.AddedFivegnodeList();
                GetMainSignals().UpdatedFivegnode(*pmn);
                mapSeenFivegnodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "UpdateFivegnodeList");
    }
}

bool CFivegnodeMan::CheckMnbAndUpdateFivegnodeList(CNode* pfrom, CFivegnodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK(cs_main);

    {
        LOCK(cs);
        nDos = 0;
        LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- fivegnode=%s\n", mnb.vin.prevout.ToStringShort());

        uint256 hash = mnb.GetHash();
        if (mapSeenFivegnodeBroadcast.count(hash) && !mnb.fRecovery) { //seen
            LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- fivegnode=%s seen\n", mnb.vin.prevout.ToStringShort());
            // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
            if (GetTime() - mapSeenFivegnodeBroadcast[hash].first > FIVEGNODE_NEW_START_REQUIRED_SECONDS - FIVEGNODE_MIN_MNP_SECONDS * 2) {
                LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- fivegnode=%s seen update\n", mnb.vin.prevout.ToStringShort());
                mapSeenFivegnodeBroadcast[hash].first = GetTime();
                fivegnodeSync.AddedFivegnodeList();
                GetMainSignals().UpdatedFivegnode(mnb);
            }
            // did we ask this node for it?
            if (pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first) {
                LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- mnb=%s seen request\n", hash.ToString());
                if (mMnbRecoveryRequests[hash].second.count(pfrom->addr)) {
                    LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- mnb=%s seen request, addr=%s\n", hash.ToString(), pfrom->addr.ToString());
                    // do not allow node to send same mnb multiple times in recovery mode
                    mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                    // does it have newer lastPing?
                    if (mnb.lastPing.sigTime > mapSeenFivegnodeBroadcast[hash].second.lastPing.sigTime) {
                        // simulate Check
                        CFivegnode mnTemp = CFivegnode(mnb);
                        mnTemp.Check();
                        LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n", hash.ToString(), pfrom->addr.ToString(), (GetTime() - mnb.lastPing.sigTime) / 60, mnTemp.GetStateString());
                        if (mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState)) {
                            // this node thinks it's a good one
                            LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- fivegnode=%s seen good\n", mnb.vin.prevout.ToStringShort());
                            mMnbRecoveryGoodReplies[hash].push_back(mnb);
                        }
                    }
                }
            }
            return true;
        }
        mapSeenFivegnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

        LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- fivegnode=%s new\n", mnb.vin.prevout.ToStringShort());

        if (!mnb.SimpleCheck(nDos)) {
            LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- SimpleCheck() failed, fivegnode=%s\n", mnb.vin.prevout.ToStringShort());
            return false;
        }

        // search Fivegnode list
        CFivegnode *pmn = Find(mnb.vin);
        if (pmn) {
            CFivegnodeBroadcast mnbOld = mapSeenFivegnodeBroadcast[CFivegnodeBroadcast(*pmn).GetHash()].second;
            if (!mnb.Update(pmn, nDos)) {
                LogPrint("fivegnode", "CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- Update() failed, fivegnode=%s\n", mnb.vin.prevout.ToStringShort());
                return false;
            }
            if (hash != mnbOld.GetHash()) {
                mapSeenFivegnodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } // end of LOCK(cs);

    if(mnb.CheckOutpoint(nDos)) {
        if(Add(mnb)){
            GetMainSignals().UpdatedFivegnode(mnb);  
        }
        fivegnodeSync.AddedFivegnodeList();
        // if it matches our Fivegnode privkey...
        if(fFivegNode && mnb.pubKeyFivegnode == activeFivegnode.pubKeyFivegnode) {
            mnb.nPoSeBanScore = -FIVEGNODE_POSE_BAN_MAX_SCORE;
            if(mnb.nProtocolVersion == PROTOCOL_VERSION) {
                // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                LogPrintf("CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- Got NEW Fivegnode entry: fivegnode=%s  sigTime=%lld  addr=%s\n",
                            mnb.vin.prevout.ToStringShort(), mnb.sigTime, mnb.addr.ToString());
                activeFivegnode.ManageState();
            } else {
                // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                // but also do not ban the node we get this message from
                LogPrintf("CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                return false;
            }
        }
        mnb.RelayFivegNode();
    } else {
        LogPrintf("CFivegnodeMan::CheckMnbAndUpdateFivegnodeList -- Rejected Fivegnode entry: %s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());
        return false;
    }

    return true;
}

void CFivegnodeMan::UpdateLastPaid()
{
    LOCK(cs);
    if(fLiteMode) return;
    if(!pCurrentBlockIndex) {
        // LogPrintf("CFivegnodeMan::UpdateLastPaid, pCurrentBlockIndex=NULL\n");
        return;
    }

    static bool IsFirstRun = true;
    // Do full scan on first run or if we are not a fivegnode
    // (MNs should update this info on every block, so limited scan should be enough for them)
    int nMaxBlocksToScanBack = (IsFirstRun || !fFivegNode) ? mnpayments.GetStorageLimit() : LAST_PAID_SCAN_BLOCKS;

    LogPrint("mnpayments", "CFivegnodeMan::UpdateLastPaid -- nHeight=%d, nMaxBlocksToScanBack=%d, IsFirstRun=%s\n",
                             pCurrentBlockIndex->nHeight, nMaxBlocksToScanBack, IsFirstRun ? "true" : "false");

    BOOST_FOREACH(CFivegnode& mn, vFivegnodes) {
        mn.UpdateLastPaid(pCurrentBlockIndex, nMaxBlocksToScanBack);
    }

    // every time is like the first time if winners list is not synced
    IsFirstRun = !fivegnodeSync.IsWinnersListSynced();
}

void CFivegnodeMan::CheckAndRebuildFivegnodeIndex()
{
    LOCK(cs);

    if(GetTime() - nLastIndexRebuildTime < MIN_INDEX_REBUILD_TIME) {
        return;
    }

    if(indexFivegnodes.GetSize() <= MAX_EXPECTED_INDEX_SIZE) {
        return;
    }

    if(indexFivegnodes.GetSize() <= int(vFivegnodes.size())) {
        return;
    }

    indexFivegnodesOld = indexFivegnodes;
    indexFivegnodes.Clear();
    for(size_t i = 0; i < vFivegnodes.size(); ++i) {
        indexFivegnodes.AddFivegnodeVIN(vFivegnodes[i].vin);
    }

    fIndexRebuilt = true;
    nLastIndexRebuildTime = GetTime();
}

void CFivegnodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CFivegnodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any fivegnodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= FIVEGNODE_WATCHDOG_MAX_SECONDS;
}

void CFivegnodeMan::CheckFivegnode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CFivegnodeMan::CheckFivegnode(const CPubKey& pubKeyFivegnode, bool fForce)
{
    LOCK(cs);
    CFivegnode* pMN = Find(pubKeyFivegnode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CFivegnodeMan::GetFivegnodeState(const CTxIn& vin)
{
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    if(!pMN)  {
        return CFivegnode::FIVEGNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CFivegnodeMan::GetFivegnodeState(const CPubKey& pubKeyFivegnode)
{
    LOCK(cs);
    CFivegnode* pMN = Find(pubKeyFivegnode);
    if(!pMN)  {
        return CFivegnode::FIVEGNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CFivegnodeMan::IsFivegnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CFivegnodeMan::SetFivegnodeLastPing(const CTxIn& vin, const CFivegnodePing& mnp)
{
    LOCK(cs);
    CFivegnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->SetLastPing(mnp);
    mapSeenFivegnodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CFivegnodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenFivegnodeBroadcast.count(hash)) {
        mapSeenFivegnodeBroadcast[hash].second.SetLastPing(mnp);
    }
}

void CFivegnodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    LogPrint("fivegnode", "CFivegnodeMan::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    CheckSameAddr();
    
    if(fFivegNode) {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        UpdateLastPaid();
    }
}

void CFivegnodeMan::NotifyFivegnodeUpdates()
{
    // Avoid double locking
    bool fFivegnodesAddedLocal = false;
    bool fFivegnodesRemovedLocal = false;
    {
        LOCK(cs);
        fFivegnodesAddedLocal = fFivegnodesAdded;
        fFivegnodesRemovedLocal = fFivegnodesRemoved;
    }

    if(fFivegnodesAddedLocal) {
//        governance.CheckFivegnodeOrphanObjects();
//        governance.CheckFivegnodeOrphanVotes();
    }
    if(fFivegnodesRemovedLocal) {
//        governance.UpdateCachesAndClean();
    }

    LOCK(cs);
    fFivegnodesAdded = false;
    fFivegnodesRemoved = false;
}
