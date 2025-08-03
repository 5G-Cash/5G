// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validation.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "checkqueue.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "init.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "random.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "timedata.h"
#include "tinyformat.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "undo.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "versionbits.h"
#include "evm/evm.h"



// Global EVM state used for experimental execution
static EVMState g_evmState;
extern CBlockTreeDB* pblocktree;

static bool IsEVMTransaction(const CTransaction& tx, CEVMTransaction& evmTx)
{
    if (tx.vout.empty() || !tx.vout[0].scriptPubKey.IsUnspendable())
        return false;
    std::vector<unsigned char> script(tx.vout[0].scriptPubKey.begin(), tx.vout[0].scriptPubKey.end());
    if (script.empty() || script[0] != OP_RETURN)
        return false;
    std::vector<unsigned char> payload(script.begin() + 1, script.end());
    CDataStream ss(payload, SER_NETWORK, 0);
    try {
        ss >> evmTx;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ProcessEVMTransaction(const CTransaction& tx)
{
    CEVMTransaction evmTx;
    if (!IsEVMTransaction(tx, evmTx))
        return false;
    EVM engine;
    CAmount gasUsed;
    std::vector<unsigned char> out;
    if (!engine.Execute(evmTx, g_evmState, gasUsed, out))
        return false;
    pblocktree->WriteEVMAccount(evmTx.to, g_evmState.GetOrCreate(evmTx.to));
    pblocktree->WriteEVMReceipt(tx.GetHash(), out);
    return true;
}

