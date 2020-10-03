// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activefivegnode.h"
#include "checkpoints.h"
#include "main.h"
#include "fivegnode.h"
#include "fivegnode-payments.h"
#include "fivegnode-sync.h"
#include "fivegnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"
#include "validationinterface.h"

class CFivegnodeSync;

CFivegnodeSync fivegnodeSync;

bool CFivegnodeSync::CheckNodeHeight(CNode *pnode, bool fDisconnectStuckNodes) {
    CNodeStateStats stats;
    if (!GetNodeStateStats(pnode->id, stats) || stats.nCommonHeight == -1 || stats.nSyncHeight == -1) return false; // not enough info about this peer

    // Check blocks and headers, allow a small error margin of 1 block
    if (pCurrentBlockIndex->nHeight - 1 > stats.nCommonHeight) {
        // This peer probably stuck, don't sync any additional data from it
        if (fDisconnectStuckNodes) {
            // Disconnect to free this connection slot for another peer.
            pnode->fDisconnect = true;
            LogPrintf("CFivegnodeSync::CheckNodeHeight -- disconnecting from stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->id);
        } else {
            LogPrintf("CFivegnodeSync::CheckNodeHeight -- skipping stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->id);
        }
        return false;
    } else if (pCurrentBlockIndex->nHeight < stats.nSyncHeight - 1) {
        // This peer announced more headers than we have blocks currently
        LogPrint("fivegnode", "CFivegnodeSync::CheckNodeHeight -- skipping peer, who announced more headers than we have blocks currently, nHeight=%d, nSyncHeight=%d, peer=%d\n",
                  pCurrentBlockIndex->nHeight, stats.nSyncHeight, pnode->id);
        return false;
    }

    return true;
}

bool CFivegnodeSync::GetBlockchainSynced(bool fBlockAccepted){
    bool currentBlockchainSynced = fBlockchainSynced;
    IsBlockchainSynced(fBlockAccepted);
    if(currentBlockchainSynced != fBlockchainSynced){
        GetMainSignals().UpdateSyncStatus();
    }
    return fBlockchainSynced;
}

bool CFivegnodeSync::IsBlockchainSynced(bool fBlockAccepted) {
    static int64_t nTimeLastProcess = GetTime();
    static int nSkipped = 0;
    static bool fFirstBlockAccepted = false;

    // If the last call to this function was more than 60 minutes ago 
    // (client was in sleep mode) reset the sync process
    if (GetTime() - nTimeLastProcess > 60 * 60) {
        LogPrintf("CFivegnodeSync::IsBlockchainSynced time-check fBlockchainSynced=%s\n", 
                  fBlockchainSynced);
        Reset();
        fBlockchainSynced = false;
    }

    if (!pCurrentBlockIndex || !pindexBestHeader || fImporting || fReindex) 
        return false;

    if (fBlockAccepted) {
        // This should be only triggered while we are still syncing.
        if (!IsSynced()) {
            // We are trying to download smth, reset blockchain sync status.
            fFirstBlockAccepted = true;
            fBlockchainSynced = false;
            nTimeLastProcess = GetTime();
            return false;
        }
    } else {
        // Dont skip on REGTEST to make the tests run faster.
        if(Params().NetworkIDString() != CBaseChainParams::REGTEST) {
            // skip if we already checked less than 1 tick ago.
            if (GetTime() - nTimeLastProcess < FIVEGNODE_SYNC_TICK_SECONDS) {
                nSkipped++;
                return fBlockchainSynced;
            }
        }
    }

    LogPrint("fivegnode-sync", 
             "CFivegnodeSync::IsBlockchainSynced -- state before check: %ssynced, skipped %d times\n", 
             fBlockchainSynced ? "" : "not ", 
             nSkipped);

    nTimeLastProcess = GetTime();
    nSkipped = 0;

    if (fBlockchainSynced){
        return true;
    }

    if (fCheckpointsEnabled && 
        pCurrentBlockIndex->nHeight < Checkpoints::GetTotalBlocksEstimate(Params().Checkpoints())) {
        
        return false;
    }

    std::vector < CNode * > vNodesCopy = CopyNodeVector();
    // We have enough peers and assume most of them are synced
    if (vNodesCopy.size() >= FIVEGNODE_SYNC_ENOUGH_PEERS) {
        // Check to see how many of our peers are (almost) at the same height as we are
        int nNodesAtSameHeight = 0;
        BOOST_FOREACH(CNode * pnode, vNodesCopy)
        {
            // Make sure this peer is presumably at the same height
            if (!CheckNodeHeight(pnode)) {
                continue;
            }
            nNodesAtSameHeight++;
            // if we have decent number of such peers, most likely we are synced now
            if (nNodesAtSameHeight >= FIVEGNODE_SYNC_ENOUGH_PEERS) {
                LogPrintf("CFivegnodeSync::IsBlockchainSynced -- found enough peers on the same height as we are, done\n");
                fBlockchainSynced = true;
                ReleaseNodeVector(vNodesCopy);
                return fBlockchainSynced;
            }
        }
    }
    ReleaseNodeVector(vNodesCopy);

    // wait for at least one new block to be accepted
    if (!fFirstBlockAccepted){ 
        fBlockchainSynced = false;
        return false;
    }

    // same as !IsInitialBlockDownload() but no cs_main needed here
    int64_t nMaxBlockTime = std::max(pCurrentBlockIndex->GetBlockTime(), pindexBestHeader->GetBlockTime());
    fBlockchainSynced = pindexBestHeader->nHeight - pCurrentBlockIndex->nHeight < 24 * 6 &&
                        GetTime() - nMaxBlockTime < Params().MaxTipAge();
    return fBlockchainSynced;
}

void CFivegnodeSync::Fail() {
    nTimeLastFailure = GetTime();
    nRequestedFivegnodeAssets = FIVEGNODE_SYNC_FAILED;
    GetMainSignals().UpdateSyncStatus();
}

void CFivegnodeSync::Reset() {
    nRequestedFivegnodeAssets = FIVEGNODE_SYNC_INITIAL;
    nRequestedFivegnodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    nTimeLastFivegnodeList = GetTime();
    nTimeLastPaymentVote = GetTime();
    nTimeLastGovernanceItem = GetTime();
    nTimeLastFailure = 0;
    nCountFailures = 0;
}

std::string CFivegnodeSync::GetAssetName() {
    switch (nRequestedFivegnodeAssets) {
        case (FIVEGNODE_SYNC_INITIAL):
            return "FIVEGNODE_SYNC_INITIAL";
        case (FIVEGNODE_SYNC_SPORKS):
            return "FIVEGNODE_SYNC_SPORKS";
        case (FIVEGNODE_SYNC_LIST):
            return "FIVEGNODE_SYNC_LIST";
        case (FIVEGNODE_SYNC_MNW):
            return "FIVEGNODE_SYNC_MNW";
        case (FIVEGNODE_SYNC_FAILED):
            return "FIVEGNODE_SYNC_FAILED";
        case FIVEGNODE_SYNC_FINISHED:
            return "FIVEGNODE_SYNC_FINISHED";
        default:
            return "UNKNOWN";
    }
}

void CFivegnodeSync::SwitchToNextAsset() {
    switch (nRequestedFivegnodeAssets) {
        case (FIVEGNODE_SYNC_FAILED):
            throw std::runtime_error("Can't switch to next asset from failed, should use Reset() first!");
            break;
        case (FIVEGNODE_SYNC_INITIAL):
            ClearFulfilledRequests();
            nRequestedFivegnodeAssets = FIVEGNODE_SYNC_SPORKS;
            LogPrintf("CFivegnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (FIVEGNODE_SYNC_SPORKS):
            nTimeLastFivegnodeList = GetTime();
            nRequestedFivegnodeAssets = FIVEGNODE_SYNC_LIST;
            LogPrintf("CFivegnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (FIVEGNODE_SYNC_LIST):
            nTimeLastPaymentVote = GetTime();
            nRequestedFivegnodeAssets = FIVEGNODE_SYNC_MNW;
            LogPrintf("CFivegnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;

        case (FIVEGNODE_SYNC_MNW):
            nTimeLastGovernanceItem = GetTime();
            LogPrintf("CFivegnodeSync::SwitchToNextAsset -- Sync has finished\n");
            nRequestedFivegnodeAssets = FIVEGNODE_SYNC_FINISHED;
            break;
    }
    nRequestedFivegnodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    GetMainSignals().UpdateSyncStatus();
}

std::string CFivegnodeSync::GetSyncStatus() {
    switch (fivegnodeSync.nRequestedFivegnodeAssets) {
        case FIVEGNODE_SYNC_INITIAL:
            return _("Synchronization pending...");
        case FIVEGNODE_SYNC_SPORKS:
            return _("Synchronizing sporks...");
        case FIVEGNODE_SYNC_LIST:
            return _("Synchronizing fivegnodes...");
        case FIVEGNODE_SYNC_MNW:
            return _("Synchronizing fivegnode payments...");
        case FIVEGNODE_SYNC_FAILED:
            return _("Synchronization failed");
        case FIVEGNODE_SYNC_FINISHED:
            return _("Synchronization finished");
        default:
            return "";
    }
}

void CFivegnodeSync::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {
    if (strCommand == NetMsgType::SYNCSTATUSCOUNT) { //Sync status count

        //do not care about stats if sync process finished or failed
        if (IsSynced() || IsFailed()) return;

        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        LogPrintf("SYNCSTATUSCOUNT -- got inventory count: nItemID=%d  nCount=%d  peer=%d\n", nItemID, nCount, pfrom->id);
    }
}

void CFivegnodeSync::ClearFulfilledRequests() {
    TRY_LOCK(cs_vNodes, lockRecv);
    if (!lockRecv) return;

    BOOST_FOREACH(CNode * pnode, vNodes)
    {
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "spork-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "fivegnode-list-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "fivegnode-payment-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "full-sync");
    }
}

void CFivegnodeSync::ProcessTick() {
    static int nTick = 0;
    if (nTick++ % FIVEGNODE_SYNC_TICK_SECONDS != 0) return;
    if (!pCurrentBlockIndex) return;

    //the actual count of fivegnodes we have currently
    int nMnCount = mnodeman.CountFivegnodes();

    LogPrint("ProcessTick", "CFivegnodeSync::ProcessTick -- nTick %d nMnCount %d\n", nTick, nMnCount);

    // INITIAL SYNC SETUP / LOG REPORTING
    double nSyncProgress = double(nRequestedFivegnodeAttempt + (nRequestedFivegnodeAssets - 1) * 8) / (8 * 4);
    LogPrint("ProcessTick", "CFivegnodeSync::ProcessTick -- nTick %d nRequestedFivegnodeAssets %d nRequestedFivegnodeAttempt %d nSyncProgress %f\n", nTick, nRequestedFivegnodeAssets, nRequestedFivegnodeAttempt, nSyncProgress);
    uiInterface.NotifyAdditionalDataSyncProgressChanged(pCurrentBlockIndex->nHeight, nSyncProgress);

    // RESET SYNCING INCASE OF FAILURE
    {
        if (IsSynced()) {
            /*
                Resync if we lost all fivegnodes from sleep/wake or failed to sync originally
            */
            if (nMnCount == 0) {
                LogPrintf("CFivegnodeSync::ProcessTick -- WARNING: not enough data, restarting sync\n");
                Reset();
            } else {
                std::vector < CNode * > vNodesCopy = CopyNodeVector();
                ReleaseNodeVector(vNodesCopy);
                return;
            }
        }

        //try syncing again
        if (IsFailed()) {
            if (nTimeLastFailure + (1 * 60) < GetTime()) { // 1 minute cooldown after failed sync
                Reset();
            }
            return;
        }
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !IsBlockchainSynced() && nRequestedFivegnodeAssets > FIVEGNODE_SYNC_SPORKS) {
        nTimeLastFivegnodeList = GetTime();
        nTimeLastPaymentVote = GetTime();
        nTimeLastGovernanceItem = GetTime();
        return;
    }
    if (nRequestedFivegnodeAssets == FIVEGNODE_SYNC_INITIAL || (nRequestedFivegnodeAssets == FIVEGNODE_SYNC_SPORKS && IsBlockchainSynced())) {
        SwitchToNextAsset();
    }

    std::vector < CNode * > vNodesCopy = CopyNodeVector();

    BOOST_FOREACH(CNode * pnode, vNodesCopy)
    {
        // Don't try to sync any data from outbound "fivegnode" connections -
        // they are temporary and should be considered unreliable for a sync process.
        // Inbound connection this early is most likely a "fivegnode" connection
        // initialted from another node, so skip it too.
        if (pnode->fFivegnode || (fFivegNode && pnode->fInbound)) continue;

        // QUICK MODE (REGTEST ONLY!)
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
            if (nRequestedFivegnodeAttempt <= 2) {
                pnode->PushMessage(NetMsgType::GETSPORKS); //get current network sporks
            } else if (nRequestedFivegnodeAttempt < 4) {
                mnodeman.DsegUpdate(pnode);
            } else if (nRequestedFivegnodeAttempt < 6) {
                int nMnCount = mnodeman.CountFivegnodes();
                pnode->PushMessage(NetMsgType::FIVEGNODEPAYMENTSYNC, nMnCount); //sync payment votes
            } else {
                nRequestedFivegnodeAssets = FIVEGNODE_SYNC_FINISHED;
                GetMainSignals().UpdateSyncStatus();
            }
            nRequestedFivegnodeAttempt++;
            ReleaseNodeVector(vNodesCopy);
            return;
        }

        // NORMAL NETWORK MODE - TESTNET/MAINNET
        {
            if (netfulfilledman.HasFulfilledRequest(pnode->addr, "full-sync")) {
                // We already fully synced from this node recently,
                // disconnect to free this connection slot for another peer.
                pnode->fDisconnect = true;
                LogPrintf("CFivegnodeSync::ProcessTick -- disconnecting from recently synced peer %d\n", pnode->id);
                continue;
            }

            // SPORK : ALWAYS ASK FOR SPORKS AS WE SYNC (we skip this mode now)

            if (!netfulfilledman.HasFulfilledRequest(pnode->addr, "spork-sync")) {
                // only request once from each peer
                netfulfilledman.AddFulfilledRequest(pnode->addr, "spork-sync");
                // get current network sporks
                pnode->PushMessage(NetMsgType::GETSPORKS);
                LogPrintf("CFivegnodeSync::ProcessTick -- nTick %d nRequestedFivegnodeAssets %d -- requesting sporks from peer %d\n", nTick, nRequestedFivegnodeAssets, pnode->id);
                continue; // always get sporks first, switch to the next node without waiting for the next tick
            }

            // MNLIST : SYNC FIVEGNODE LIST FROM OTHER CONNECTED CLIENTS

            if (nRequestedFivegnodeAssets == FIVEGNODE_SYNC_LIST) {
                // check for timeout first
                if (nTimeLastFivegnodeList < GetTime() - FIVEGNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrintf("CFivegnodeSync::ProcessTick -- nTick %d nRequestedFivegnodeAssets %d -- timeout\n", nTick, nRequestedFivegnodeAssets);
                    if (nRequestedFivegnodeAttempt == 0) {
                        LogPrintf("CFivegnodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // there is no way we can continue without fivegnode list, fail here and try later
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "fivegnode-list-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "fivegnode-list-sync");

                if (pnode->nVersion < mnpayments.GetMinFivegnodePaymentsProto()) continue;
                nRequestedFivegnodeAttempt++;

                mnodeman.DsegUpdate(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // MNW : SYNC FIVEGNODE PAYMENT VOTES FROM OTHER CONNECTED CLIENTS

            if (nRequestedFivegnodeAssets == FIVEGNODE_SYNC_MNW) {
                LogPrint("mnpayments", "CFivegnodeSync::ProcessTick -- nTick %d nRequestedFivegnodeAssets %d nTimeLastPaymentVote %lld GetTime() %lld diff %lld\n", nTick, nRequestedFivegnodeAssets, nTimeLastPaymentVote, GetTime(), GetTime() - nTimeLastPaymentVote);
                // check for timeout first
                // This might take a lot longer than FIVEGNODE_SYNC_TIMEOUT_SECONDS minutes due to new blocks,
                // but that should be OK and it should timeout eventually.
                if (nTimeLastPaymentVote < GetTime() - FIVEGNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrintf("CFivegnodeSync::ProcessTick -- nTick %d nRequestedFivegnodeAssets %d -- timeout\n", nTick, nRequestedFivegnodeAssets);
                    if (nRequestedFivegnodeAttempt == 0) {
                        LogPrintf("CFivegnodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // probably not a good idea to proceed without winner list
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // check for data
                // if mnpayments already has enough blocks and votes, switch to the next asset
                // try to fetch data from at least two peers though
                if (nRequestedFivegnodeAttempt > 1 && mnpayments.IsEnoughData()) {
                    LogPrintf("CFivegnodeSync::ProcessTick -- nTick %d nRequestedFivegnodeAssets %d -- found enough data\n", nTick, nRequestedFivegnodeAssets);
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "fivegnode-payment-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "fivegnode-payment-sync");

                if (pnode->nVersion < mnpayments.GetMinFivegnodePaymentsProto()) continue;
                nRequestedFivegnodeAttempt++;

                // ask node for all payment votes it has (new nodes will only return votes for future payments)
                pnode->PushMessage(NetMsgType::FIVEGNODEPAYMENTSYNC, mnpayments.GetStorageLimit());
                // ask node for missing pieces only (old nodes will not be asked)
                mnpayments.RequestLowDataPaymentBlocks(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

        }
    }
    // looped through all nodes, release them
    ReleaseNodeVector(vNodesCopy);
}

void CFivegnodeSync::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
}
