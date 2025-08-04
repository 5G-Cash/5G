#include "evm.h"
#include "../serialize.h"
#include "../streams.h"
#include "../hash.h"
#include "../arith_uint256.h"

uint256 EVMState::GetHash() const {
    return SerializeHash(accounts);
}

bool EVM::Execute(const CEVMTransaction& tx, EVMState& state, uint64_t& gasUsed, std::vector<unsigned char>& output) {
    gasUsed = 0;

    if (!state.Transfer(uint160(), tx.to, tx.value))
        return false;

    std::vector<uint256> stack;
    const std::vector<unsigned char>& code = tx.data;
    EVMAccount& acct = state.GetOrCreate(tx.to);

    for (size_t pc = 0; pc < code.size(); ) {
        if (gasUsed >= tx.gasLimit)
            return false;

        unsigned char op = code[pc++];
        gasUsed++;

        switch (op) {
        case 0x00: // STOP
            pc = code.size();
            break;
        case 0x01: { // ADD
            if (stack.size() < 2) return false;
            arith_uint256 a = UintToArith256(stack.back()); stack.pop_back();
            arith_uint256 b = UintToArith256(stack.back()); stack.pop_back();
            stack.push_back(ArithToUint256(a + b));
            break; }
        case 0x54: { // SLOAD
            if (stack.empty()) return false;
            uint256 key = stack.back(); stack.pop_back();
            auto it = acct.storage.find(key);
            stack.push_back(it != acct.storage.end() ? it->second : uint256());
            break; }
        case 0x55: { // SSTORE
            if (stack.size() < 2) return false;
            uint256 key = stack.back(); stack.pop_back();
            uint256 val = stack.back(); stack.pop_back();
            acct.storage[key] = val;
            break; }
        case 0x60: { // PUSH1
            if (pc >= code.size()) return false;
            arith_uint256 v(code[pc++]);
            stack.push_back(ArithToUint256(v));
            break; }
        default:
            return false;
        }
    }

    if (!stack.empty()) {
        CDataStream ss(SER_NETWORK, 0);
        ss << stack.back();
        output.assign(ss.begin(), ss.end());
    } else {
        output.clear();
    }

    return true;
}
