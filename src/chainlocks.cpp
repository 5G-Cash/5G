#include "chainlocks.h"
#include "chain.h"
#include "main.h"

CChainLocks g_chainlocks;

void CChainLocks::ProcessNewChainLock(int nHeight, const uint256& hash, const std::vector<unsigned char>& vchSig)
{
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
    return pindex && pindex->fChainLocked;
}

int CChainLocks::GetBestChainLockHeight() const
{
    LOCK(cs);
    return nBestChainLockHeight;
}

uint256 CChainLocks::GetBestChainLockHash() const
{
    LOCK(cs);
    return hashBestChainLock;
}
