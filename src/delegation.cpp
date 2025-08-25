// Copyright (c) 2025 The FiveG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegation.h"

// In-memory delegation map.
// TODO: Persist and populate via transactions or RPC calls.
std::map<CScriptID, CScriptID> mapStakeDelegations;

bool IsDelegatedStake(const CScript& owner, const CScript& delegate)
{
    auto it = mapStakeDelegations.find(CScriptID(owner));
    if (it == mapStakeDelegations.end())
        return false;
    return it->second == CScriptID(delegate);
}

void RegisterDelegation(const CScript& owner, const CScript& delegate)
{
    mapStakeDelegations[CScriptID(owner)] = CScriptID(delegate);
}

void RemoveDelegation(const CScript& owner)
{
    mapStakeDelegations.erase(CScriptID(owner));
}
