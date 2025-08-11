// Copyright (c) 2025 The FiveG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FIVEG_DELEGATION_H
#define FIVEG_DELEGATION_H

#include <map>
#include "script/script.h"
#include "script/standard.h"

/** Simple registry mapping coin owner scripts to delegated staking scripts. */
extern std::map<CScriptID, CScript> mapStakeDelegations;

/**
 * Returns true if the provided delegate script is authorized to stake coins
 * owned by the given script.
 */
bool IsDelegatedStake(const CScript& owner, const CScript& delegate);

/** Register a delegate script for an owner script. */
void RegisterDelegation(const CScript& owner, const CScript& delegate);

/** Remove an existing delegation for an owner script. */
void RemoveDelegation(const CScript& owner);

#endif // FIVEG_DELEGATION_H
