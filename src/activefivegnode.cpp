// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activefivegnode.h"
#include "consensus/consensus.h"
#include "fivegnode.h"
#include "fivegnode-sync.h"
#include "fivegnode-payments.h"
#include "fivegnodeman.h"
#include "protocol.h"
#include "validationinterface.h"

extern CWallet *pwalletMain;

// Keep track of the active Fivegnode
CActiveFivegnode activeFivegnode;

void CActiveFivegnode::ManageState() {
    LogPrint("fivegnode", "CActiveFivegnode::ManageState -- Start\n");
    if (!fFivegNode) {
        LogPrint("fivegnode", "CActiveFivegnode::ManageState -- Not a fivegnode, returning\n");
        return;
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !fivegnodeSync.GetBlockchainSynced()) {
        ChangeState(ACTIVE_FIVEGNODE_SYNC_IN_PROCESS);
        LogPrintf("CActiveFivegnode::ManageState -- %s: %s\n", GetStateString(), GetStatus());
        return;
    }

    if (nState == ACTIVE_FIVEGNODE_SYNC_IN_PROCESS) {
        ChangeState(ACTIVE_FIVEGNODE_INITIAL);
    }

    LogPrint("fivegnode", "CActiveFivegnode::ManageState -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    if (eType == FIVEGNODE_UNKNOWN) {
        ManageStateInitial();
    }

    if (eType == FIVEGNODE_REMOTE) {
        ManageStateRemote();
    } else if (eType == FIVEGNODE_LOCAL) {
        // Try Remote Start first so the started local fivegnode can be restarted without recreate fivegnode broadcast.
        ManageStateRemote();
        if (nState != ACTIVE_FIVEGNODE_STARTED)
            ManageStateLocal();
    }

    SendFivegnodePing();
}

std::string CActiveFivegnode::GetStateString() const {
    switch (nState) {
        case ACTIVE_FIVEGNODE_INITIAL:
            return "INITIAL";
        case ACTIVE_FIVEGNODE_SYNC_IN_PROCESS:
            return "SYNC_IN_PROCESS";
        case ACTIVE_FIVEGNODE_INPUT_TOO_NEW:
            return "INPUT_TOO_NEW";
        case ACTIVE_FIVEGNODE_NOT_CAPABLE:
            return "NOT_CAPABLE";
        case ACTIVE_FIVEGNODE_STARTED:
            return "STARTED";
        default:
            return "UNKNOWN";
    }
}

void CActiveFivegnode::ChangeState(int state) {
    if(nState!=state){
        nState = state;
    }
}

std::string CActiveFivegnode::GetStatus() const {
    switch (nState) {
        case ACTIVE_FIVEGNODE_INITIAL:
            return "Node just started, not yet activated";
        case ACTIVE_FIVEGNODE_SYNC_IN_PROCESS:
            return "Sync in progress. Must wait until sync is complete to start Fivegnode";
        case ACTIVE_FIVEGNODE_INPUT_TOO_NEW:
            return strprintf("Fivegnode input must have at least %d confirmations",
                             Params().GetConsensus().nFivegnodeMinimumConfirmations);
        case ACTIVE_FIVEGNODE_NOT_CAPABLE:
            return "Not capable fivegnode: " + strNotCapableReason;
        case ACTIVE_FIVEGNODE_STARTED:
            return "Fivegnode successfully started";
        default:
            return "Unknown";
    }
}

std::string CActiveFivegnode::GetTypeString() const {
    std::string strType;
    switch (eType) {
        case FIVEGNODE_UNKNOWN:
            strType = "UNKNOWN";
            break;
        case FIVEGNODE_REMOTE:
            strType = "REMOTE";
            break;
        case FIVEGNODE_LOCAL:
            strType = "LOCAL";
            break;
        default:
            strType = "UNKNOWN";
            break;
    }
    return strType;
}

bool CActiveFivegnode::SendFivegnodePing() {
    if (!fPingerEnabled) {
        LogPrint("fivegnode",
                 "CActiveFivegnode::SendFivegnodePing -- %s: fivegnode ping service is disabled, skipping...\n",
                 GetStateString());
        return false;
    }

    if (!mnodeman.Has(vin)) {
        strNotCapableReason = "Fivegnode not in fivegnode list";
        ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
        LogPrintf("CActiveFivegnode::SendFivegnodePing -- %s: %s\n", GetStateString(), strNotCapableReason);
        return false;
    }

    CFivegnodePing mnp(vin);
    if (!mnp.Sign(keyFivegnode, pubKeyFivegnode)) {
        LogPrintf("CActiveFivegnode::SendFivegnodePing -- ERROR: Couldn't sign Fivegnode Ping\n");
        return false;
    }

    // Update lastPing for our fivegnode in Fivegnode list
    if (mnodeman.IsFivegnodePingedWithin(vin, FIVEGNODE_MIN_MNP_SECONDS, mnp.sigTime)) {
        LogPrintf("CActiveFivegnode::SendFivegnodePing -- Too early to send Fivegnode Ping\n");
        return false;
    }

    mnodeman.SetFivegnodeLastPing(vin, mnp);

    LogPrintf("CActiveFivegnode::SendFivegnodePing -- Relaying ping, collateral=%s\n", vin.ToString());
    mnp.Relay();

    return true;
}

void CActiveFivegnode::ManageStateInitial() {
    LogPrint("fivegnode", "CActiveFivegnode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
        strNotCapableReason = "Fivegnode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CFivegnode::IsValidNetAddr(service);
        if (!fFoundLocal) {
            // nothing and no live connections, can't do anything for now
            if (vNodes.empty()) {
                ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            BOOST_FOREACH(CNode * pnode, vNodes)
            {
                if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4()) {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CFivegnode::IsValidNetAddr(service);
                    if (fFoundLocal) break;
                }
            }
        }
    }

    if (!fFoundLocal) {
        ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(),
                                            mainnetDefaultPort);
            LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(),
                                        mainnetDefaultPort);
        LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    LogPrintf("CActiveFivegnode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString());
    //TODO
    if (!ConnectNode(CAddress(service, NODE_NETWORK), NULL, false, true)) {
        ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
        strNotCapableReason = "Could not connect to " + service.ToString();
        LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // Default to REMOTE
    eType = FIVEGNODE_REMOTE;

    // Check if wallet funds are available
    if (!pwalletMain) {
        LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: Wallet not available\n", GetStateString());
        return;
    }

    if (pwalletMain->IsLocked()) {
        LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString());
        return;
    }

    if (pwalletMain->GetBalance() < FIVEGNODE_COIN_REQUIRED * COIN) {
        LogPrintf("CActiveFivegnode::ManageStateInitial -- %s: Wallet balance is < 10000 FIVEG\n", GetStateString());
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode
    if (pwalletMain->GetFivegnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        eType = FIVEGNODE_LOCAL;
    }

    LogPrint("fivegnode", "CActiveFivegnode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
}

void CActiveFivegnode::ManageStateRemote() {
    LogPrint("fivegnode",
             "CActiveFivegnode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyFivegnode.GetID() = %s\n",
             GetStatus(), fPingerEnabled, GetTypeString(), pubKeyFivegnode.GetID().ToString());

    mnodeman.CheckFivegnode(pubKeyFivegnode);
    fivegnode_info_t infoMn = mnodeman.GetFivegnodeInfo(pubKeyFivegnode);

    if (infoMn.fInfoValid) {
        if (infoMn.nProtocolVersion < MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_1 || infoMn.nProtocolVersion > MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_2) {
            ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
            strNotCapableReason = "Invalid protocol version";
            LogPrintf("CActiveFivegnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (service != infoMn.addr) {
            ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
            // LogPrintf("service: %s\n", service.ToString());
            // LogPrintf("infoMn.addr: %s\n", infoMn.addr.ToString());
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this fivegnode changed recently.";
            LogPrintf("CActiveFivegnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (!CFivegnode::IsValidStateForAutoStart(infoMn.nActiveState)) {
            ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
            strNotCapableReason = strprintf("Fivegnode in %s state", CFivegnode::StateToString(infoMn.nActiveState));
            LogPrintf("CActiveFivegnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (nState != ACTIVE_FIVEGNODE_STARTED) {
            LogPrintf("CActiveFivegnode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            ChangeState(ACTIVE_FIVEGNODE_STARTED);
        }
    } else {
        ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
        strNotCapableReason = "Fivegnode not in fivegnode list";
        LogPrintf("CActiveFivegnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
    }
}

void CActiveFivegnode::ManageStateLocal() {
    LogPrint("fivegnode", "CActiveFivegnode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
    if (nState == ACTIVE_FIVEGNODE_STARTED) {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if (pwalletMain->GetFivegnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge < Params().GetConsensus().nFivegnodeMinimumConfirmations) {
            ChangeState(ACTIVE_FIVEGNODE_INPUT_TOO_NEW);
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            LogPrintf("CActiveFivegnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);
        }

        CFivegnodeBroadcast mnb;
        std::string strError;
        if (!CFivegnodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keyFivegnode,
                                     pubKeyFivegnode, strError, mnb)) {
            ChangeState(ACTIVE_FIVEGNODE_NOT_CAPABLE);
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            LogPrintf("CActiveFivegnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        fPingerEnabled = true;
        ChangeState(ACTIVE_FIVEGNODE_STARTED);

        //update to fivegnode list
        LogPrintf("CActiveFivegnode::ManageStateLocal -- Update Fivegnode List\n");
        mnodeman.UpdateFivegnodeList(mnb);
        mnodeman.NotifyFivegnodeUpdates();

        //send to all peers
        LogPrintf("CActiveFivegnode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString());
        mnb.RelayFivegNode();
    }
}
