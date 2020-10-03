// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FIVEGNODE_PAYMENTS_H
#define FIVEGNODE_PAYMENTS_H

#include "util.h"
#include "core_io.h"
#include "key.h"
#include "main.h"
#include "fivegnode.h"
#include "utilstrencodings.h"

class CFivegnodePayments;
class CFivegnodePaymentVote;
class CFivegnodeBlockPayees;

static const int MNPAYMENTS_SIGNATURES_REQUIRED         = 6;
static const int MNPAYMENTS_SIGNATURES_TOTAL            = 10;

//! minimum peer version that can receive and send fivegnode payment messages,
//  vote for fivegnode and be elected as a payment winner
// V1 - Last protocol version before update
// V2 - Newest protocol version
static const int MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_1 = MIN_PEER_PROTO_VERSION;
static const int MIN_FIVEGNODE_PAYMENT_PROTO_VERSION_2 = PROTOCOL_VERSION;

extern CCriticalSection cs_vecPayees;
extern CCriticalSection cs_mapFivegnodeBlocks;
extern CCriticalSection cs_mapFivegnodePayeeVotes;

extern CFivegnodePayments mnpayments;

/// TODO: all 4 functions do not belong here really, they should be refactored/moved somewhere (main.cpp ?)
bool IsBlockValueValid(const CBlock& block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet);
bool IsBlockPayeeValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward);
void FillBlockPayments(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutFivegnodeRet, std::vector<CTxOut>& voutSuperblockRet);
std::string GetRequiredPaymentsString(int nBlockHeight);

class CFivegnodePayee
{
private:
    CScript scriptPubKey;
    std::vector<uint256> vecVoteHashes;

public:
    CFivegnodePayee() :
        scriptPubKey(),
        vecVoteHashes()
        {}

    CFivegnodePayee(CScript payee, uint256 hashIn) :
        scriptPubKey(payee),
        vecVoteHashes()
    {
        vecVoteHashes.push_back(hashIn);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CScriptBase*)(&scriptPubKey));
        READWRITE(vecVoteHashes);
    }

    CScript GetPayee() { return scriptPubKey; }

    void AddVoteHash(uint256 hashIn) { vecVoteHashes.push_back(hashIn); }
    std::vector<uint256> GetVoteHashes() { return vecVoteHashes; }
    int GetVoteCount() { return vecVoteHashes.size(); }
    std::string ToString() const;
};

// Keep track of votes for payees from fivegnodes
class CFivegnodeBlockPayees
{
public:
    int nBlockHeight;
    std::vector<CFivegnodePayee> vecPayees;

    CFivegnodeBlockPayees() :
        nBlockHeight(0),
        vecPayees()
        {}
    CFivegnodeBlockPayees(int nBlockHeightIn) :
        nBlockHeight(nBlockHeightIn),
        vecPayees()
        {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nBlockHeight);
        READWRITE(vecPayees);
    }

    void AddPayee(const CFivegnodePaymentVote& vote);
    bool GetBestPayee(CScript& payeeRet);
    bool HasPayeeWithVotes(CScript payeeIn, int nVotesReq);

    bool IsTransactionValid(const CTransaction& txNew, bool fMTP, int nHeight);

    std::string GetRequiredPaymentsString();
};

// vote for the winning payment
class CFivegnodePaymentVote
{
public:
    CTxIn vinFivegnode;

    int nBlockHeight;
    CScript payee;
    std::vector<unsigned char> vchSig;

    CFivegnodePaymentVote() :
        vinFivegnode(),
        nBlockHeight(0),
        payee(),
        vchSig()
        {}

    CFivegnodePaymentVote(CTxIn vinFivegnode, int nBlockHeight, CScript payee) :
        vinFivegnode(vinFivegnode),
        nBlockHeight(nBlockHeight),
        payee(payee),
        vchSig()
        {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vinFivegnode);
        READWRITE(nBlockHeight);
        READWRITE(*(CScriptBase*)(&payee));
        READWRITE(vchSig);
    }

    uint256 GetHash() const {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << *(CScriptBase*)(&payee);
        ss << nBlockHeight;
        ss << vinFivegnode.prevout;
        return ss.GetHash();
    }

    bool Sign();
    bool CheckSignature(const CPubKey& pubKeyFivegnode, int nValidationHeight, int &nDos);

    bool IsValid(CNode* pnode, int nValidationHeight, std::string& strError);
    void Relay();

    bool IsVerified() { return !vchSig.empty(); }
    void MarkAsNotVerified() { vchSig.clear(); }

    std::string ToString() const;
};

//
// Fivegnode Payments Class
// Keeps track of who should get paid for which blocks
//

class CFivegnodePayments
{
private:
    // fivegnode count times nStorageCoeff payments blocks should be stored ...
    const float nStorageCoeff;
    // ... but at least nMinBlocksToStore (payments blocks)
    const int nMinBlocksToStore;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

public:
    std::map<uint256, CFivegnodePaymentVote> mapFivegnodePaymentVotes;
    std::map<int, CFivegnodeBlockPayees> mapFivegnodeBlocks;
    std::map<COutPoint, int> mapFivegnodesLastVote;

    CFivegnodePayments() : nStorageCoeff(1.25), nMinBlocksToStore(5000) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(mapFivegnodePaymentVotes);
        READWRITE(mapFivegnodeBlocks);
    }

    void Clear();

    bool AddPaymentVote(const CFivegnodePaymentVote& vote);
    bool HasVerifiedPaymentVote(uint256 hashIn);
    bool ProcessBlock(int nBlockHeight);

    void Sync(CNode* node);
    void RequestLowDataPaymentBlocks(CNode* pnode);
    void CheckAndRemove();

    bool GetBlockPayee(int nBlockHeight, CScript& payee);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight, bool fMTP);
    bool IsScheduled(CFivegnode& mn, int nNotBlockHeight);

    bool CanVote(COutPoint outFivegnode, int nBlockHeight);

    int GetMinFivegnodePaymentsProto();
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutFivegnodeRet);
    std::string ToString() const;

    int GetBlockCount() { return mapFivegnodeBlocks.size(); }
    int GetVoteCount() { return mapFivegnodePaymentVotes.size(); }

    bool IsEnoughData();
    int GetStorageLimit();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
