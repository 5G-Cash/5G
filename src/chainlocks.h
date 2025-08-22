#ifndef BITCOIN_CHAINLOCKS_H
#define BITCOIN_CHAINLOCKS_H

#include "uint256.h"
#include "sync.h"
#include <vector>

class CBlockIndex;

// ChainLocks quorum parameters
static const size_t CHAINLOCKS_QUORUM_SIZE = 5;
static const size_t CHAINLOCKS_THRESHOLD   = 3;

class CChainLocks {
public:
    void ProcessNewChainLock(int nHeight, const uint256& hash, const std::vector<unsigned char>& vchSig);
    bool VerifyChainLockSignature(int nHeight, const uint256& hash, const std::vector<unsigned char>& vchSig);
    bool IsBlockChainLocked(const CBlockIndex* pindex) const;
    int GetBestChainLockHeight() const;
    uint256 GetBestChainLockHash() const;
private:
    mutable CCriticalSection cs;
    int nBestChainLockHeight = -1;
    uint256 hashBestChainLock;
};

extern CChainLocks g_chainlocks;

#endif // BITCOIN_CHAINLOCKS_H
