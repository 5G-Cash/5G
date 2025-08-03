#ifndef BITCOIN_EVM_EVM_H
#define BITCOIN_EVM_EVM_H

#include <map>
#include <vector>

#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"
#include "amount.h"

struct EVMAccount {
    uint64_t nonce;
    CAmount balance;
    std::map<uint256, uint256> storage;
    std::vector<unsigned char> code;

    EVMAccount() : nonce(0), balance(0) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nonce);
        READWRITE(balance);
        READWRITE(storage);
        READWRITE(code);
    }
};

class EVMState {
    std::map<uint160, EVMAccount> accounts;
public:
    EVMAccount& GetOrCreate(const uint160& addr) {
        return accounts[addr];
    }

    bool Transfer(const uint160& from, const uint160& to, CAmount value) {
        EVMAccount& src = GetOrCreate(from);
        EVMAccount& dst = GetOrCreate(to);
        if (src.balance < value)
            return false;
        src.balance -= value;
        dst.balance += value;
        return true;
    }
};

class EVM {
public:
    bool Execute(const CEVMTransaction& tx, EVMState& state, CAmount& gasUsed, std::vector<unsigned char>& output) {
        gasUsed = 0;
        if (!state.Transfer(uint160(), tx.to, tx.value))
            return false;
        output = tx.data; // placeholder for opcode execution
        return true;
    }
};

#endif // BITCOIN_EVM_EVM_H
