#ifndef FIVEG_DELEGATION_H
#define FIVEG_DELEGATION_H

// Copyright (c) 2025 The FiveG developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>
#include "script/script.h"

/** Simple registry mapping coin owner scripts to delegated staking scripts. */
extern std::map<CScript, CScript> mapStakeDelegations;

/**
 * Returns true if the provided delegate script is authorized to stake coins
 * owned by the given script.
 */
bool IsDelegatedStake(const CScript& owner, const CScript& delegate);

#endif // FIVEG_DELEGATION_H
