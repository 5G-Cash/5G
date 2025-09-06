#ifndef BITCOIN_FINALITY_H
#define BITCOIN_FINALITY_H

#include "uint256.h"
#include "sync.h"
#include <map>
#include <set>

class CBlockIndex;

struct ValidatorVote {
    int nHeight;
    uint256 blockHash;
};

class FinalityManager {
public:
    void RegisterVote(const uint256& validator, const ValidatorVote& vote);
    int GetFinalizedHeight() const;
    bool IsBlockFinalized(const CBlockIndex* pindex) const;
private:
    mutable CCriticalSection cs;
    std::map<int, std::set<uint256>> mapVotes;
    int nFinalizedHeight = -1;
};

extern FinalityManager g_finalityman;

#endif // BITCOIN_FINALITY_H
