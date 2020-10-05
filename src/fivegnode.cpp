// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activefivegnode.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "darksend.h"
#include "init.h"
//#include "governance.h"
#include "fivegnode.h"
#include "fivegnode-payments.h"
#include "fivegnodeconfig.h"
#include "fivegnode-sync.h"
#include "fivegnodeman.h"
#include "util.h"
#include "validationinterface.h"

#include <boost/lexical_cast.hpp>


CFivegnode::CFivegnode() :
        vin(),
        addr(),
        pubKeyCollateralAddress(),
        pubKeyFivegnode(),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(FIVEGNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(PROTOCOL_VERSION),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CFivegnode::CFivegnode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyFivegnodeNew, int nProtocolVersionIn) :
        vin(vinNew),
        addr(addrNew),
        pubKeyCollateralAddress(pubKeyCollateralAddressNew),
        pubKeyFivegnode(pubKeyFivegnodeNew),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(FIVEGNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(nProtocolVersionIn),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CFivegnode::CFivegnode(const CFivegnode &other) :
        vin(other.vin),
        addr(other.addr),
        pubKeyCollateralAddress(other.pubKeyCollateralAddress),
        pubKeyFivegnode(other.pubKeyFivegnode),
        lastPing(other.lastPing),
        vchSig(other.vchSig),
        sigTime(other.sigTime),
        nLastDsq(other.nLastDsq),
        nTimeLastChecked(other.nTimeLastChecked),
        nTimeLastPaid(other.nTimeLastPaid),
        nTimeLastWatchdogVote(other.nTimeLastWatchdogVote),
        nActiveState(other.nActiveState),
        nCacheCollateralBlock(other.nCacheCollateralBlock),
        nBlockLastPaid(other.nBlockLastPaid),
        nProtocolVersion(other.nProtocolVersion),
        nPoSeBanScore(other.nPoSeBanScore),
        nPoSeBanHeight(other.nPoSeBanHeight),
        fAllowMixingTx(other.fAllowMixingTx),
        fUnitTest(other.fUnitTest) {}

CFivegnode::CFivegnode(const CFivegnodeBroadcast &mnb) :
        vin(mnb.vin),
        addr(mnb.addr),
        pubKeyCollateralAddress(mnb.pubKeyCollateralAddress),
        pubKeyFivegnode(mnb.pubKeyFivegnode),
        lastPing(mnb.lastPing),
        vchSig(mnb.vchSig),
        sigTime(mnb.sigTime),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(mnb.sigTime),
        nActiveState(mnb.nActiveState),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(mnb.nProtocolVersion),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

//CSporkManager sporkManager;
//
// When a new fivegnode broadcast is sent, update our information
//
bool CFivegnode::UpdateFromNewBroadcast(CFivegnodeBroadcast &mnb) {
    if (mnb.sigTime <= sigTime && !mnb.fRecovery) return false;

    pubKeyFivegnode = mnb.pubKeyFivegnode;
    sigTime = mnb.sigTime;
    vchSig = mnb.vchSig;
    nProtocolVersion = mnb.nProtocolVersion;
    addr = mnb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
    int nDos = 0;
    if (mnb.lastPing == CFivegnodePing() || (mnb.lastPing != CFivegnodePing() && mnb.lastPing.CheckAndUpdate(this, true, nDos))) {
        SetLastPing(mnb.lastPing);
        mnodeman.mapSeenFivegnodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our Fivegnode privkey...
    if (fFivegNode && pubKeyFivegnode == activeFivegnode.pubKeyFivegnode) {
        nPoSeBanScore = -FIVEGNODE_POSE_BAN_MAX_SCORE;
        if (nProtocolVersion == PROTOCOL_VERSION) {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activeFivegnode.ManageState();
        } else {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            LogPrintf("CFivegnode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a Fivegnode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CFivegnode::CalculateScore(const uint256 &blockHash) {
    uint256 aux = ArithToUint256(UintToArith256(vin.prevout.hash) + vin.prevout.n);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << blockHash;
    arith_uint256 hash2 = UintToArith256(ss.GetHash());

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << blockHash;
    ss2 << aux;
    arith_uint256 hash3 = UintToArith256(ss2.GetHash());

    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
}

void CFivegnode::Check(bool fForce) {
    LOCK(cs);

    if (ShutdownRequested()) return;

    if (!fForce && (GetTime() - nTimeLastChecked < FIVEGNODE_CHECK_SECONDS)) return;
    nTimeLastChecked = GetTime();

    LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state\n", vin.prevout.ToStringShort(), GetStateString());

    //once spent, stop doing the checks
    if (IsOutpointSpent()) return;

    int nHeight = 0;
    if (!fUnitTest) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) return;

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            SetStatus(FIVEGNODE_OUTPOINT_SPENT);
            LogPrint("fivegnode", "CFivegnode::Check -- Failed to find Fivegnode UTXO, fivegnode=%s\n", vin.prevout.ToStringShort());
            return;
        }

        nHeight = chainActive.Height();
    }

    if (IsPoSeBanned()) {
        if (nHeight < nPoSeBanHeight) return; // too early?
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // Fivegnode still will be on the edge and can be banned back easily if it keeps ignoring mnverify
        // or connect attempts. Will require few mnverify messages to strengthen its position in mn list.
        LogPrintf("CFivegnode::Check -- Fivegnode %s is unbanned and back in list now\n", vin.prevout.ToStringShort());
        DecreasePoSeBanScore();
    } else if (nPoSeBanScore >= FIVEGNODE_POSE_BAN_MAX_SCORE) {
        SetStatus(FIVEGNODE_POSE_BAN);
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + mnodeman.size();
        LogPrintf("CFivegnode::Check -- Fivegnode %s is banned till block %d now\n", vin.prevout.ToStringShort(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurFivegnode = fFivegNode && activeFivegnode.pubKeyFivegnode == pubKeyFivegnode;

    // fivegnode doesn't meet payment protocol requirements ...
/*    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinFivegnodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurFivegnode && nProtocolVersion < PROTOCOL_VERSION); */

    // fivegnode doesn't meet payment protocol requirements ...
    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinFivegnodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurFivegnode && (nProtocolVersion < MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_1 || nProtocolVersion > MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_2));

    if (fRequireUpdate) {
        SetStatus(FIVEGNODE_UPDATE_REQUIRED);
        if (nActiveStatePrev != nActiveState) {
            LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    // keep old fivegnodes on start, give them a chance to receive updates...
    bool fWaitForPing = !fivegnodeSync.IsFivegnodeListSynced() && !IsPingedWithin(FIVEGNODE_MIN_MNP_SECONDS);

    if (fWaitForPing && !fOurFivegnode) {
        // ...but if it was already expired before the initial check - return right away
        if (IsExpired() || IsWatchdogExpired() || IsNewStartRequired()) {
            LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state, waiting for ping\n", vin.prevout.ToStringShort(), GetStateString());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own fivegnode
    if (!fWaitForPing || fOurFivegnode) {

        if (!IsPingedWithin(FIVEGNODE_NEW_START_REQUIRED_SECONDS)) {
            SetStatus(FIVEGNODE_NEW_START_REQUIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        bool fWatchdogActive = fivegnodeSync.IsSynced() && mnodeman.IsWatchdogActive();
        bool fWatchdogExpired = (fWatchdogActive && ((GetTime() - nTimeLastWatchdogVote) > FIVEGNODE_WATCHDOG_MAX_SECONDS));

//        LogPrint("fivegnode", "CFivegnode::Check -- outpoint=%s, nTimeLastWatchdogVote=%d, GetTime()=%d, fWatchdogExpired=%d\n",
//                vin.prevout.ToStringShort(), nTimeLastWatchdogVote, GetTime(), fWatchdogExpired);

        if (fWatchdogExpired) {
            SetStatus(FIVEGNODE_WATCHDOG_EXPIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        if (!IsPingedWithin(FIVEGNODE_EXPIRATION_SECONDS)) {
            SetStatus(FIVEGNODE_EXPIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }
    }

    if (lastPing.sigTime - sigTime < FIVEGNODE_MIN_MNP_SECONDS) {
        SetStatus(FIVEGNODE_PRE_ENABLED);
        if (nActiveStatePrev != nActiveState) {
            LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    SetStatus(FIVEGNODE_ENABLED); // OK
    if (nActiveStatePrev != nActiveState) {
        LogPrint("fivegnode", "CFivegnode::Check -- Fivegnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
    }
}

bool CFivegnode::IsValidNetAddr() {
    return IsValidNetAddr(addr);
}

bool CFivegnode::IsValidForPayment() {
    if (nActiveState == FIVEGNODE_ENABLED) {
        return true;
    }
//    if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
//       (nActiveState == FIVEGNODE_WATCHDOG_EXPIRED)) {
//        return true;
//    }

    return false;
}

bool CFivegnode::IsValidNetAddr(CService addrIn) {
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkIDString() == CBaseChainParams::REGTEST ||
           (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}

bool CFivegnode::IsMyFivegnode(){
    BOOST_FOREACH(CFivegnodeConfig::CFivegnodeEntry mne, fivegnodeConfig.getEntries()) {
        const std::string& txHash = mne.getTxHash();
        const std::string& outputIndex = mne.getOutputIndex();

        if(txHash==vin.prevout.hash.ToString().substr(0,64) &&
           outputIndex==to_string(vin.prevout.n))
            return true;
    }
    return false;
}

fivegnode_info_t CFivegnode::GetInfo() {
    fivegnode_info_t info;
    info.vin = vin;
    info.addr = addr;
    info.pubKeyCollateralAddress = pubKeyCollateralAddress;
    info.pubKeyFivegnode = pubKeyFivegnode;
    info.sigTime = sigTime;
    info.nLastDsq = nLastDsq;
    info.nTimeLastChecked = nTimeLastChecked;
    info.nTimeLastPaid = nTimeLastPaid;
    info.nTimeLastWatchdogVote = nTimeLastWatchdogVote;
    info.nTimeLastPing = lastPing.sigTime;
    info.nActiveState = nActiveState;
    info.nProtocolVersion = nProtocolVersion;
    info.fInfoValid = true;
    return info;
}

std::string CFivegnode::StateToString(int nStateIn) {
    switch (nStateIn) {
        case FIVEGNODE_PRE_ENABLED:
            return "PRE_ENABLED";
        case FIVEGNODE_ENABLED:
            return "ENABLED";
        case FIVEGNODE_EXPIRED:
            return "EXPIRED";
        case FIVEGNODE_OUTPOINT_SPENT:
            return "OUTPOINT_SPENT";
        case FIVEGNODE_UPDATE_REQUIRED:
            return "UPDATE_REQUIRED";
        case FIVEGNODE_WATCHDOG_EXPIRED:
            return "WATCHDOG_EXPIRED";
        case FIVEGNODE_NEW_START_REQUIRED:
            return "NEW_START_REQUIRED";
        case FIVEGNODE_POSE_BAN:
            return "POSE_BAN";
        default:
            return "UNKNOWN";
    }
}

std::string CFivegnode::GetStateString() const {
    return StateToString(nActiveState);
}

std::string CFivegnode::GetStatus() const {
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

void CFivegnode::SetStatus(int newState) {
    if(nActiveState!=newState){
        nActiveState = newState;
        if(IsMyFivegnode())
            GetMainSignals().UpdatedFivegnode(*this);
    }
}

void CFivegnode::SetLastPing(CFivegnodePing newFivegnodePing) {
    if(lastPing!=newFivegnodePing){
        lastPing = newFivegnodePing;
        if(IsMyFivegnode())
            GetMainSignals().UpdatedFivegnode(*this);
    }
}

void CFivegnode::SetTimeLastPaid(int64_t newTimeLastPaid) {
     if(nTimeLastPaid!=newTimeLastPaid){
        nTimeLastPaid = newTimeLastPaid;
        if(IsMyFivegnode())
            GetMainSignals().UpdatedFivegnode(*this);
    }   
}

void CFivegnode::SetBlockLastPaid(int newBlockLastPaid) {
     if(nBlockLastPaid!=newBlockLastPaid){
        nBlockLastPaid = newBlockLastPaid;
        if(IsMyFivegnode())
            GetMainSignals().UpdatedFivegnode(*this);
    }   
}

void CFivegnode::SetRank(int newRank) {
     if(nRank!=newRank){
        nRank = newRank;
        if(nRank < 0 || nRank > mnodeman.size()) nRank = 0;
        if(IsMyFivegnode())
            GetMainSignals().UpdatedFivegnode(*this);
    }   
}

std::string CFivegnode::ToString() const {
    std::string str;
    str += "fivegnode{";
    str += addr.ToString();
    str += " ";
    str += std::to_string(nProtocolVersion);
    str += " ";
    str += vin.prevout.ToStringShort();
    str += " ";
    str += CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    str += " ";
    str += std::to_string(lastPing == CFivegnodePing() ? sigTime : lastPing.sigTime);
    str += " ";
    str += std::to_string(lastPing == CFivegnodePing() ? 0 : lastPing.sigTime - sigTime);
    str += " ";
    str += std::to_string(nBlockLastPaid);
    str += "}\n";
    return str;
}

UniValue CFivegnode::ToJSON() const {
    UniValue ret(UniValue::VOBJ);
    std::string payee = CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    COutPoint outpoint = vin.prevout;
    UniValue outpointObj(UniValue::VOBJ);
    UniValue authorityObj(UniValue::VOBJ);
    outpointObj.push_back(Pair("txid", outpoint.hash.ToString().substr(0,64)));
    outpointObj.push_back(Pair("index", to_string(outpoint.n)));

    std::string authority = addr.ToString();
    std::string ip   = authority.substr(0, authority.find(":"));
    std::string port = authority.substr(authority.find(":")+1, authority.length());
    authorityObj.push_back(Pair("ip", ip));
    authorityObj.push_back(Pair("port", port));
    
    // get myFivegnode data
    bool isMine = false;
    string label;
    int fIndex=0;
    BOOST_FOREACH(CFivegnodeConfig::CFivegnodeEntry mne, fivegnodeConfig.getEntries()) {
        CTxIn myVin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
        if(outpoint.ToStringShort()==myVin.prevout.ToStringShort()){
            isMine = true;
            label = mne.getAlias();
            break;
        }
        fIndex++;
    }

    ret.push_back(Pair("rank", nRank));
    ret.push_back(Pair("outpoint", outpointObj));
    ret.push_back(Pair("status", GetStatus()));
    ret.push_back(Pair("protocolVersion", nProtocolVersion));
    ret.push_back(Pair("payeeAddress", payee));
    ret.push_back(Pair("lastSeen", (int64_t) lastPing.sigTime * 1000));
    ret.push_back(Pair("activeSince", (int64_t)(sigTime * 1000)));
    ret.push_back(Pair("lastPaidTime", (int64_t) GetLastPaidTime() * 1000));
    ret.push_back(Pair("lastPaidBlock", GetLastPaidBlock()));
    ret.push_back(Pair("authority", authorityObj));
    ret.push_back(Pair("isMine", isMine));
    if(isMine){
        ret.push_back(Pair("label", label));
        ret.push_back(Pair("position", fIndex));
    }

    UniValue qualify(UniValue::VOBJ);

    CFivegnode* fivegnode = const_cast <CFivegnode*> (this);
    qualify = mnodeman.GetNotQualifyReasonToUniValue(*fivegnode, chainActive.Tip()->nHeight, true, mnodeman.CountEnabled());
    ret.push_back(Pair("qualify", qualify));

    return ret;
}

int CFivegnode::GetCollateralAge() {
    int nHeight;
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain || !chainActive.Tip()) return -1;
        nHeight = chainActive.Height();
    }

    if (nCacheCollateralBlock == 0) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge > 0) {
            nCacheCollateralBlock = nHeight - nInputAge;
        } else {
            return nInputAge;
        }
    }

    return nHeight - nCacheCollateralBlock;
}

void CFivegnode::UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack) {
    if (!pindex) {
        LogPrintf("CFivegnode::UpdateLastPaid pindex is NULL\n");
        return;
    }

    const Consensus::Params &params = Params().GetConsensus();
    const CBlockIndex *BlockReading = pindex;

    CScript mnpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());
    LogPrint("fivegnode", "CFivegnode::UpdateLastPaidBlock -- searching for block with payment to %s\n", vin.prevout.ToStringShort());

    LOCK(cs_mapFivegnodeBlocks);

    for (int i = 0; BlockReading && BlockReading->nHeight > nBlockLastPaid && i < nMaxBlocksToScanBack; i++) {
//        LogPrintf("mnpayments.mapFivegnodeBlocks.count(BlockReading->nHeight)=%s\n", mnpayments.mapFivegnodeBlocks.count(BlockReading->nHeight));
//        LogPrintf("mnpayments.mapFivegnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)=%s\n", mnpayments.mapFivegnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2));
        if (mnpayments.mapFivegnodeBlocks.count(BlockReading->nHeight) &&
            mnpayments.mapFivegnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)) {
            // LogPrintf("i=%s, BlockReading->nHeight=%s\n", i, BlockReading->nHeight);
            CBlock block;
            if (!ReadBlockFromDisk(block, BlockReading, Params().GetConsensus())) // shouldn't really happen
            {
                LogPrintf("ReadBlockFromDisk failed\n");
                continue;
            }
            CAmount nFivegnodePayment = GetFivegnodePayment(params, false,BlockReading->nHeight);

            BOOST_FOREACH(CTxOut txout, block.vtx[0].vout)
            if (mnpayee == txout.scriptPubKey && nFivegnodePayment == txout.nValue) {
                SetBlockLastPaid(BlockReading->nHeight);
                SetTimeLastPaid(BlockReading->nTime);
                LogPrint("fivegnode", "CFivegnode::UpdateLastPaidBlock -- searching for block with payment to %s -- found new %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
                return;
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    // Last payment for this fivegnode wasn't found in latest mnpayments blocks
    // or it was found in mnpayments blocks but wasn't found in the blockchain.
    // LogPrint("fivegnode", "CFivegnode::UpdateLastPaidBlock -- searching for block with payment to %s -- keeping old %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
}

bool CFivegnodeBroadcast::Create(std::string strService, std::string strKeyFivegnode, std::string strTxHash, std::string strOutputIndex, std::string &strErrorRet, CFivegnodeBroadcast &mnbRet, bool fOffline) {
    LogPrintf("CFivegnodeBroadcast::Create\n");
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyFivegnodeNew;
    CKey keyFivegnodeNew;
    //need correct blocks to send ping
    if (!fOffline && !fivegnodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Fivegnode";
        LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    //TODO
    if (!darkSendSigner.GetKeysFromSecret(strKeyFivegnode, keyFivegnodeNew, pubKeyFivegnodeNew)) {
        strErrorRet = strprintf("Invalid fivegnode key %s", strKeyFivegnode);
        LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!pwalletMain->GetFivegnodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for fivegnode %s", strTxHash, strOutputIndex, strService);
        LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    CService service = CService(strService);
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            strErrorRet = strprintf("Invalid port %u for fivegnode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
            LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
            return false;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for fivegnode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
        LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    return Create(txin, CService(strService), keyCollateralAddressNew, pubKeyCollateralAddressNew, keyFivegnodeNew, pubKeyFivegnodeNew, strErrorRet, mnbRet);
}

bool CFivegnodeBroadcast::Create(CTxIn txin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyFivegnodeNew, CPubKey pubKeyFivegnodeNew, std::string &strErrorRet, CFivegnodeBroadcast &mnbRet) {
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    LogPrint("fivegnode", "CFivegnodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyFivegnodeNew.GetID() = %s\n",
             CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
             pubKeyFivegnodeNew.GetID().ToString());


    CFivegnodePing mnp(txin);
    if (!mnp.Sign(keyFivegnodeNew, pubKeyFivegnodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, fivegnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CFivegnodeBroadcast();
        return false;
    }

    int nHeight = chainActive.Height();
    if (nHeight < ZC_MODULUS_V2_START_BLOCK) {
        mnbRet = CFivegnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyFivegnodeNew, MIN_PEER_PROTO_VERSION);
    } else {
        mnbRet = CFivegnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyFivegnodeNew, PROTOCOL_VERSION);
    }

    if (!mnbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address, fivegnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CFivegnodeBroadcast();
        return false;
    }
    mnbRet.SetLastPing(mnp);
    if (!mnbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, fivegnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CFivegnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CFivegnodeBroadcast();
        return false;
    }

    return true;
}

bool CFivegnodeBroadcast::SimpleCheck(int &nDos) {
    nDos = 0;

    // make sure addr is valid
    if (!IsValidNetAddr()) {
        LogPrintf("CFivegnodeBroadcast::SimpleCheck -- Invalid addr, rejected: fivegnode=%s  addr=%s\n",
                  vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CFivegnodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: fivegnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if (lastPing == CFivegnodePing() || !lastPing.SimpleCheck(nDos)) {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        SetStatus(FIVEGNODE_EXPIRED);
    }

    if (nProtocolVersion < mnpayments.GetMinFivegnodePaymentsProto()) {
        LogPrintf("CFivegnodeBroadcast::SimpleCheck -- ignoring outdated Fivegnode: fivegnode=%s  nProtocolVersion=%d\n", vin.prevout.ToStringShort(), nProtocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrintf("CFivegnodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyFivegnode.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrintf("CFivegnodeBroadcast::SimpleCheck -- pubKeyFivegnode has the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!vin.scriptSig.empty()) {
        LogPrintf("CFivegnodeBroadcast::SimpleCheck -- Ignore Not Empty ScriptSig %s\n", vin.ToString());
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != mainnetDefaultPort) return false;
    } else if (addr.GetPort() == mainnetDefaultPort) return false;

    return true;
}

bool CFivegnodeBroadcast::Update(CFivegnode *pmn, int &nDos) {
    nDos = 0;

    if (pmn->sigTime == sigTime && !fRecovery) {
        // mapSeenFivegnodeBroadcast in CFivegnodeMan::CheckMnbAndUpdateFivegnodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if (pmn->sigTime > sigTime) {
        LogPrintf("CFivegnodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for Fivegnode %s %s\n",
                  sigTime, pmn->sigTime, vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    pmn->Check();

    // fivegnode is banned by PoSe
    if (pmn->IsPoSeBanned()) {
        LogPrintf("CFivegnodeBroadcast::Update -- Banned by PoSe, fivegnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if (pmn->pubKeyCollateralAddress != pubKeyCollateralAddress) {
        LogPrintf("CFivegnodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CFivegnodeBroadcast::Update -- CheckSignature() failed, fivegnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // if ther was no fivegnode broadcast recently or if it matches our Fivegnode privkey...
    if (!pmn->IsBroadcastedWithin(FIVEGNODE_MIN_MNB_SECONDS) || (fFivegNode && pubKeyFivegnode == activeFivegnode.pubKeyFivegnode)) {
        // take the newest entry
        LogPrintf("CFivegnodeBroadcast::Update -- Got UPDATED Fivegnode entry: addr=%s\n", addr.ToString());
        if (pmn->UpdateFromNewBroadcast((*this))) {
            pmn->Check();
            RelayFivegNode();
        }
        fivegnodeSync.AddedFivegnodeList();
        GetMainSignals().UpdatedFivegnode(*pmn);
    }

    return true;
}

bool CFivegnodeBroadcast::CheckOutpoint(int &nDos) {
    // we are a fivegnode with the same vin (i.e. already activated) and this mnb is ours (matches our Fivegnode privkey)
    // so nothing to do here for us
    if (fFivegNode && vin.prevout == activeFivegnode.vin.prevout && pubKeyFivegnode == activeFivegnode.pubKeyFivegnode) {
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CFivegnodeBroadcast::CheckOutpoint -- CheckSignature() failed, fivegnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            // not mnb fault, let it to be checked again later
            LogPrint("fivegnode", "CFivegnodeBroadcast::CheckOutpoint -- Failed to aquire lock, addr=%s", addr.ToString());
            mnodeman.mapSeenFivegnodeBroadcast.erase(GetHash());
            return false;
        }

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            LogPrint("fivegnode", "CFivegnodeBroadcast::CheckOutpoint -- Failed to find Fivegnode UTXO, fivegnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (coins.vout[vin.prevout.n].nValue != FIVEGNODE_COIN_REQUIRED * COIN) {
            LogPrint("fivegnode", "CFivegnodeBroadcast::CheckOutpoint -- Fivegnode UTXO should have 50000 VGC, fivegnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (chainActive.Height() - coins.nHeight + 1 < Params().GetConsensus().nFivegnodeMinimumConfirmations) {
            LogPrintf("CFivegnodeBroadcast::CheckOutpoint -- Fivegnode UTXO must have at least %d confirmations, fivegnode=%s\n",
                      Params().GetConsensus().nFivegnodeMinimumConfirmations, vin.prevout.ToStringShort());
            // maybe we miss few blocks, let this mnb to be checked again later
            mnodeman.mapSeenFivegnodeBroadcast.erase(GetHash());
            return false;
        }
    }

    LogPrint("fivegnode", "CFivegnodeBroadcast::CheckOutpoint -- Fivegnode UTXO verified\n");

    // make sure the vout that was signed is related to the transaction that spawned the Fivegnode
    //  - this is expensive, so it's only done once per Fivegnode
    if (!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubKeyCollateralAddress)) {
        LogPrintf("CFivegnodeMan::CheckOutpoint -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 50000 VGC tx got nFivegnodeMinimumConfirmations
    uint256 hashBlock = uint256();
    CTransaction tx2;
    GetTransaction(vin.prevout.hash, tx2, Params().GetConsensus(), hashBlock, true);
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex *pMNIndex = (*mi).second; // block for 50000 VGC tx -> 1 confirmation
            CBlockIndex *pConfIndex = chainActive[pMNIndex->nHeight + Params().GetConsensus().nFivegnodeMinimumConfirmations - 1]; // block where tx got nFivegnodeMinimumConfirmations
            if (pConfIndex->GetBlockTime() > sigTime) {
                LogPrintf("CFivegnodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for Fivegnode %s %s\n",
                          sigTime, Params().GetConsensus().nFivegnodeMinimumConfirmations, pConfIndex->GetBlockTime(), vin.prevout.ToStringShort(), addr.ToString());
                return false;
            }
        }
    }

    return true;
}

bool CFivegnodeBroadcast::Sign(CKey &keyCollateralAddress) {
    std::string strError;
    std::string strMessage;

    sigTime = GetAdjustedTime();

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyFivegnode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyCollateralAddress)) {
        LogPrintf("CFivegnodeBroadcast::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CFivegnodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CFivegnodeBroadcast::CheckSignature(int &nDos) {
    std::string strMessage;
    std::string strError = "";
    nDos = 0;

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyFivegnode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    LogPrint("fivegnode", "CFivegnodeBroadcast::CheckSignature -- strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n", strMessage, CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(), EncodeBase64(&vchSig[0], vchSig.size()));

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CFivegnodeBroadcast::CheckSignature -- Got bad Fivegnode announce signature, error: %s\n", strError);
        nDos = 100;
        return false;
    }

    return true;
}

void CFivegnodeBroadcast::RelayFivegNode() {
    LogPrintf("CFivegnodeBroadcast::RelayFivegNode\n");
    CInv inv(MSG_FIVEGNODE_ANNOUNCE, GetHash());
    RelayInv(inv);
}

CFivegnodePing::CFivegnodePing(CTxIn &vinNew) {
    LOCK(cs_main);
    if (!chainActive.Tip() || chainActive.Height() < 12) return;

    vin = vinNew;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector < unsigned char > ();
}

bool CFivegnodePing::Sign(CKey &keyFivegnode, CPubKey &pubKeyFivegnode) {
    std::string strError;
    std::string strFivegNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyFivegnode)) {
        LogPrintf("CFivegnodePing::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyFivegnode, vchSig, strMessage, strError)) {
        LogPrintf("CFivegnodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CFivegnodePing::CheckSignature(CPubKey &pubKeyFivegnode, int &nDos) {
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";
    nDos = 0;

    if (!darkSendSigner.VerifyMessage(pubKeyFivegnode, vchSig, strMessage, strError)) {
        LogPrintf("CFivegnodePing::CheckSignature -- Got bad Fivegnode ping signature, fivegnode=%s, error: %s\n", vin.prevout.ToStringShort(), strError);
        nDos = 33;
        return false;
    }
    return true;
}

bool CFivegnodePing::SimpleCheck(int &nDos) {
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CFivegnodePing::SimpleCheck -- Signature rejected, too far into the future, fivegnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    {
//        LOCK(cs_main);
        AssertLockHeld(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if (mi == mapBlockIndex.end()) {
            LogPrint("fivegnode", "CFivegnodePing::SimpleCheck -- Fivegnode ping is invalid, unknown block hash: fivegnode=%s blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    LogPrint("fivegnode", "CFivegnodePing::SimpleCheck -- Fivegnode ping verified: fivegnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);
    return true;
}

bool CFivegnodePing::CheckAndUpdate(CFivegnode *pmn, bool fFromNewBroadcast, int &nDos) {
    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos)) {
        return false;
    }

    if (pmn == NULL) {
        LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- Couldn't find Fivegnode entry, fivegnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    if (!fFromNewBroadcast) {
        if (pmn->IsUpdateRequired()) {
            LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- fivegnode protocol is outdated, fivegnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (pmn->IsNewStartRequired()) {
            LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- fivegnode is completely expired, new start is required, fivegnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < chainActive.Height() - 24) {
            // LogPrintf("CFivegnodePing::CheckAndUpdate -- Fivegnode ping is invalid, block hash is too old: fivegnode=%s  blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // nDos = 1;
            return false;
        }
    }

    LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- New ping: fivegnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);

    // LogPrintf("mnping - Found corresponding mn for vin: %s\n", vin.prevout.ToStringShort());
    // update only if there is no known ping for this fivegnode or
    // last ping was more then FIVEGNODE_MIN_MNP_SECONDS-60 ago comparing to this one
    if (pmn->IsPingedWithin(FIVEGNODE_MIN_MNP_SECONDS - 60, sigTime)) {
        LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- Fivegnode ping arrived too early, fivegnode=%s\n", vin.prevout.ToStringShort());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(pmn->pubKeyFivegnode, nDos)) return false;

    // so, ping seems to be ok

    // if we are still syncing and there was no known ping for this mn for quite a while
    // (NOTE: assuming that FIVEGNODE_EXPIRATION_SECONDS/2 should be enough to finish mn list sync)
    if (!fivegnodeSync.IsFivegnodeListSynced() && !pmn->IsPingedWithin(FIVEGNODE_EXPIRATION_SECONDS / 2)) {
        // let's bump sync timeout
        LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- bumping sync timeout, fivegnode=%s\n", vin.prevout.ToStringShort());
        fivegnodeSync.AddedFivegnodeList();
        GetMainSignals().UpdatedFivegnode(*pmn);
    }

    // let's store this ping as the last one
    LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- Fivegnode ping accepted, fivegnode=%s\n", vin.prevout.ToStringShort());
    pmn->SetLastPing(*this);

    // and update mnodeman.mapSeenFivegnodeBroadcast.lastPing which is probably outdated
    CFivegnodeBroadcast mnb(*pmn);
    uint256 hash = mnb.GetHash();
    if (mnodeman.mapSeenFivegnodeBroadcast.count(hash)) {
        mnodeman.mapSeenFivegnodeBroadcast[hash].second.SetLastPing(*this);
    }

    pmn->Check(true); // force update, ignoring cache
    if (!pmn->IsEnabled()) return false;

    LogPrint("fivegnode", "CFivegnodePing::CheckAndUpdate -- Fivegnode ping acceepted and relayed, fivegnode=%s\n", vin.prevout.ToStringShort());
    Relay();

    return true;
}

void CFivegnodePing::Relay() {
    CInv inv(MSG_FIVEGNODE_PING, GetHash());
    RelayInv(inv);
}

//void CFivegnode::AddGovernanceVote(uint256 nGovernanceObjectHash)
//{
//    if(mapGovernanceObjectsVotedOn.count(nGovernanceObjectHash)) {
//        mapGovernanceObjectsVotedOn[nGovernanceObjectHash]++;
//    } else {
//        mapGovernanceObjectsVotedOn.insert(std::make_pair(nGovernanceObjectHash, 1));
//    }
//}

//void CFivegnode::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
//{
//    std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.find(nGovernanceObjectHash);
//    if(it == mapGovernanceObjectsVotedOn.end()) {
//        return;
//    }
//    mapGovernanceObjectsVotedOn.erase(it);
//}

void CFivegnode::UpdateWatchdogVoteTime() {
    LOCK(cs);
    nTimeLastWatchdogVote = GetTime();
}

/**
*   FLAG GOVERNANCE ITEMS AS DIRTY
*
*   - When fivegnode come and go on the network, we must flag the items they voted on to recalc it's cached flags
*
*/
//void CFivegnode::FlagGovernanceItemsAsDirty()
//{
//    std::vector<uint256> vecDirty;
//    {
//        std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.begin();
//        while(it != mapGovernanceObjectsVotedOn.end()) {
//            vecDirty.push_back(it->first);
//            ++it;
//        }
//    }
//    for(size_t i = 0; i < vecDirty.size(); ++i) {
//        mnodeman.AddDirtyGovernanceObjectHash(vecDirty[i]);
//    }
//}