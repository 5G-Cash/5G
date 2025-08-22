#include "finality.h"
#include "chain.h"
#include "main.h"

FinalityManager g_finalityman;

void FinalityManager::RegisterVote(const uint256& validator, const ValidatorVote& vote)
{
    LOCK(cs);
    mapVotes[vote.nHeight].insert(vote.blockHash);
    if (vote.nHeight > nFinalizedHeight) {
        nFinalizedHeight = vote.nHeight;
        BlockMap::iterator it = mapBlockIndex.find(vote.blockHash);
        if (it != mapBlockIndex.end()) {
            it->second->fFinalized = true;
        }
    }
}

int FinalityManager::GetFinalizedHeight() const
{
    LOCK(cs);
    return nFinalizedHeight;
}

bool FinalityManager::IsBlockFinalized(const CBlockIndex* pindex) const
{
    LOCK(cs);
    return pindex && pindex->fFinalized;
}
