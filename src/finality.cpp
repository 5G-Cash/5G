#include "finality.h"
#include "chain.h"
#include "main.h"
#include "spork.h"

FinalityManager g_finalityman;

void FinalityManager::RegisterVote(const uint256& validator, const ValidatorVote& vote)
{
    if (!sporkManager.IsSporkActive(SPORK_17_BFT_FINALITY_ENABLED))
        return;
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
    return sporkManager.IsSporkActive(SPORK_17_BFT_FINALITY_ENABLED) ? nFinalizedHeight : 0;
}

bool FinalityManager::IsBlockFinalized(const CBlockIndex* pindex) const
{
    LOCK(cs);
    return sporkManager.IsSporkActive(SPORK_17_BFT_FINALITY_ENABLED) && pindex && pindex->fFinalized;
}
