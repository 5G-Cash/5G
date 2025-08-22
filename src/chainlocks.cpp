#include "chainlocks.h"
#include "chain.h"
#include "main.h"
#include "spork.h"
#include "fivegnodeman.h"
#include "hash.h"
#include <algorithm>

CChainLocks g_chainlocks;

void CChainLocks::ProcessNewChainLock(int nHeight, const uint256& hash, const std::vector<unsigned char>& vchSig)
{
    if (!sporkManager.IsSporkActive(SPORK_16_CHAINLOCKS_ENABLED))
        return;
    if (!VerifyChainLockSignature(nHeight, hash, vchSig))
        return;
    LOCK(cs);
    if (nHeight <= nBestChainLockHeight)
        return;
    nBestChainLockHeight = nHeight;
    hashBestChainLock = hash;
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end()) {
        CBlockIndex* pindex = it->second;
        if (chainActive[pindex->nHeight] != pindex) {
            CValidationState state;
            ActivateBestChain(state, Params(), pindex);
        }
    }
}

bool CChainLocks::IsBlockChainLocked(const CBlockIndex* pindex) const
{
    LOCK(cs);
    return sporkManager.IsSporkActive(SPORK_16_CHAINLOCKS_ENABLED) &&
           pindex &&
           pindex->nHeight <= nBestChainLockHeight &&
           chainActive[pindex->nHeight] == pindex;
}

int CChainLocks::GetBestChainLockHeight() const
{
    LOCK(cs);
    return sporkManager.IsSporkActive(SPORK_16_CHAINLOCKS_ENABLED) ? nBestChainLockHeight : 0;
}

uint256 CChainLocks::GetBestChainLockHash() const
{
    LOCK(cs);
    return sporkManager.IsSporkActive(SPORK_16_CHAINLOCKS_ENABLED) ? hashBestChainLock : uint256();
}

bool CChainLocks::VerifyChainLockSignature(int nHeight, const uint256& hash, const std::vector<unsigned char>& vchSig)
{
    // Reconstruct the signing message
    CHashWriter ss(SER_GETHASH, 0);
    ss << nHeight;
    ss << hash;
    const uint256 sighash = ss.GetHash();

    // Select a deterministic quorum of masternodes
    std::vector<CFivegnode> vecMNs = mnodeman.GetFullFivegnodeVector();
    if (vecMNs.size() < CHAINLOCKS_QUORUM_SIZE)
        return false;

    std::sort(vecMNs.begin(), vecMNs.end(), [](const CFivegnode& a, const CFivegnode& b) {
        return a.vin.prevout < b.vin.prevout;
    });

    size_t offset = 0;
    size_t valid = 0;
    for (size_t i = 0; i < CHAINLOCKS_QUORUM_SIZE && offset < vchSig.size(); ++i) {
        if (offset + 1 > vchSig.size())
            break;
        unsigned int sigLen = vchSig[offset];
        offset++;
        if (offset + sigLen > vchSig.size())
            break;
        std::vector<unsigned char> sig(vchSig.begin() + offset, vchSig.begin() + offset + sigLen);
        offset += sigLen;

        if (vecMNs[i].pubKeyFivegnode.Verify(sighash, sig))
            valid++;
    }

    return valid >= CHAINLOCKS_THRESHOLD;
}
