// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"
#include "consensus/consensus.h"
#include "main.h"
#include "zerocoin.h"
#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"
#include "chainparams.h"
#include "crypto/scrypt.h"
#include "util.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <string>
#include "crypto/x16Rv2/hash_algos.h"

/* TODO: Change these values */
static const uint32_t MAINNET_VERUSHASH_ACTIVATIONTIME = 2000000000;
static const uint32_t TESTNET_VERUSHASH_ACTIVATIONTIME = 1646956800;
static const uint32_t REGTEST_VERUSHASH_ACTIVATIONTIME = 1629951212;

BlockNetwork bNetwork = BlockNetwork();

BlockNetwork::BlockNetwork()
{
    fOnTestnet = false;
    fOnRegtest = false;
}
void BlockNetwork::SetNetwork(const std::string& net)
{
    if (net == "test") {
        fOnTestnet = true;
    } else if (net == "regtest") {
        fOnRegtest = true;
    }
}

uint256 CBlockHeader::GetVerusHash() const
{
    return SerializeVerusHash(*this);
}

uint256 CBlockHeader::GetHash() const {
    return HashX16RV2(BEGIN(nVersion), END(nNonce), hashPrevBlock);
}

uint256 CBlockHeader::GetPoWHash() const {
    uint256 thash;
    uint32_t nTimeToUse = MAINNET_VERUSHASH_ACTIVATIONTIME;

    if (bNetwork.fOnTestnet) {
        nTimeToUse = TESTNET_VERUSHASH_ACTIVATIONTIME;
    }
    else if (bNetwork.fOnRegtest) {
        nTimeToUse = REGTEST_VERUSHASH_ACTIVATIONTIME;
    }
    else { 
        nTimeToUse = MAINNET_VERUSHASH_ACTIVATIONTIME;
    }

    if (nTime > nTimeToUse) {
        // Verushash enabled
        thash = GetVerusHash();
    }
    else {
        // X16RV2
        thash = HashX16RV2(BEGIN(nVersion), END(nNonce), hashPrevBlock);
    }

    return thash;
}

std::string CBlock::ToString() const {
    std::stringstream s;
    s << strprintf(
            "CBlock(hash=%s, powHash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
            GetHash().ToString(),
            GetPoWHash().ToString(),
            nVersion,
            hashPrevBlock.ToString(),
            hashMerkleRoot.ToString(),
            nTime, nBits, nNonce,
            vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++) {
        s << "  " << vtx[i].ToString() << "\n";
    }
    return s.str();
}
int64_t GetBlockWeight(const CBlock& block)
{
//     This implements the weight = (stripped_size * 4) + witness_size formula,
//     using only serialization with and without witness data. As witness_size
//     is equal to total_size - stripped_size, this formula is identical to:
//     weight = (stripped_size * 3) + total_size.
//    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}

void CBlock::ZerocoinClean() const {
    zerocoinTxInfo = nullptr;
}
