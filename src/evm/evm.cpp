#include "evm.h"
#include "../serialize.h"
#include "../hash.h"

uint256 EVMState::GetHash() const {
    return SerializeHash(accounts);
}
