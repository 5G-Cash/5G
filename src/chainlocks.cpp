#include "chainlocks.h"
#include "chain.h"
#include "main.h"
#include "spork.h"
#include "fivegnodeman.h"

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
    // TODO: implement real BLS verification using masternode quorums
    (void)nHeight;
    (void)hash;
    (void)vchSig;
    return true;
}
