// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef FIVEGNODE_SYNC_H
#define FIVEGNODE_SYNC_H

#include "chain.h"
#include "net.h"

#include <univalue.h>

class CFivegnodeSync;

static const int FIVEGNODE_SYNC_FAILED          = -1;
static const int FIVEGNODE_SYNC_INITIAL         = 0;
static const int FIVEGNODE_SYNC_SPORKS          = 1;
static const int FIVEGNODE_SYNC_LIST            = 2;
static const int FIVEGNODE_SYNC_MNW             = 3;
static const int FIVEGNODE_SYNC_FINISHED        = 999;

static const int FIVEGNODE_SYNC_TICK_SECONDS    = 6;
static const int FIVEGNODE_SYNC_TIMEOUT_SECONDS = 30; // our blocks are 2.5 minutes so 30 seconds should be fine

static const int FIVEGNODE_SYNC_ENOUGH_PEERS    = 3;

static bool fBlockchainSynced = false;

extern CFivegnodeSync fivegnodeSync;

//
// CFivegnodeSync : Sync fivegnode assets in stages
//

class CFivegnodeSync
{
private:
    // Keep track of current asset
    int nRequestedFivegnodeAssets;
    // Count peers we've requested the asset from
    int nRequestedFivegnodeAttempt;

    // Time when current fivegnode asset sync started
    int64_t nTimeAssetSyncStarted;

    // Last time when we received some fivegnode asset ...
    int64_t nTimeLastFivegnodeList;
    int64_t nTimeLastPaymentVote;
    int64_t nTimeLastGovernanceItem;
    // ... or failed
    int64_t nTimeLastFailure;

    // How many times we failed
    int nCountFailures;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    bool CheckNodeHeight(CNode* pnode, bool fDisconnectStuckNodes = false);
    void Fail();
    void ClearFulfilledRequests();

public:
    CFivegnodeSync() { Reset(); }

    void AddedFivegnodeList() { nTimeLastFivegnodeList = GetTime(); }
    void AddedPaymentVote() { nTimeLastPaymentVote = GetTime(); }
    void AddedGovernanceItem() { nTimeLastGovernanceItem = GetTime(); };

    void SendGovernanceSyncRequest(CNode* pnode);

    bool GetBlockchainSynced(bool fBlockAccepted = false);

    bool IsFailed() { return nRequestedFivegnodeAssets == FIVEGNODE_SYNC_FAILED; }
    bool IsBlockchainSynced(bool fBlockAccepted = false);
    bool IsFivegnodeListSynced() { return nRequestedFivegnodeAssets > FIVEGNODE_SYNC_LIST; }
    bool IsWinnersListSynced() { return nRequestedFivegnodeAssets > FIVEGNODE_SYNC_MNW; }
    bool IsSynced() { return nRequestedFivegnodeAssets == FIVEGNODE_SYNC_FINISHED; }

    int GetAssetID() { return nRequestedFivegnodeAssets; }
    int GetAttempt() { return nRequestedFivegnodeAttempt; }
    std::string GetAssetName();
    std::string GetSyncStatus();

    void Reset();
    void SwitchToNextAsset();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ProcessTick();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
