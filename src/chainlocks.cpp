#include "chainlocks.h"
#include "chain.h"
#include "main.h"
#include "spork.h"

CChainLocks g_chainlocks;

void CChainLocks::ProcessNewChainLock(int nHeight, const uint256& hash, const std::vector<unsigned char>& vchSig)
{
    if (!sporkManager.IsSporkActive(SPORK_16_CHAINLOCKS_ENABLED))
        return;
    LOCK(cs);
    if (nHeight <= nBestChainLockHeight)
        return;
    nBestChainLockHeight = nHeight;
    hashBestChainLock = hash;
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end()) {
        it->second->fChainLocked = true;
    }
}

bool CChainLocks::IsBlockChainLocked(const CBlockIndex* pindex) const
{
    LOCK(cs);
    return sporkManager.IsSporkActive(SPORK_16_CHAINLOCKS_ENABLED) && pindex && pindex->fChainLocked;
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
