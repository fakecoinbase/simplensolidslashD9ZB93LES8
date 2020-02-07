#include <cstring>
#include <stdint.h>

class uint256_t {
private:
    static constexpr int W = 8;
    uint32_t data[W];
    inline int cmp(const uint256_t& v) const {
        for (int i = 0; i < W; i++) {
            if (data[i] < v.data[i]) return -1;
            if (data[i] > v.data[i]) return 1;
        }
        return 0;
    }
public:
    inline uint256_t() { for (int i = 0; i < W; i++) data[i] = 0; }
    inline uint256_t(uint32_t v) { data[0] = v; for (int i = 1; i < W; i++) data[i] = 0; }
    inline uint256_t(const uint256_t& v) { for (int i = 0; i < W; i++) data[i] = v.data[i]; }
    inline uint256_t& operator=(const uint256_t& v) { for (int i = 0; i < W; i++) data[i] = v.data[i]; return *this; }
    inline const uint256_t operator~() const { uint256_t v; for (int i = 0; i < W; i++) v.data[i] = ~data[i]; return v; }
    inline const uint256_t operator-() const { uint256_t v = ~(*this); return ++v; }
    inline uint256_t& operator++() { for (int i = 0; i < W; i++) if (++data[i] != 0) break; return *this; }
    inline uint256_t& operator--() { for (int i = 0; i < W; i++) if (data[i]-- != 0) break; return *this; }
    inline const uint256_t operator++(int) { const uint256_t v = *this; ++(*this); return v; }
    inline const uint256_t operator--(int) { const uint256_t v = *this; --(*this); return v; }
    inline uint256_t& operator+=(const uint256_t& v)
    {
        uint64_t carry = 0;
        for (int i = 0; i < W; i++)
        {
            uint64_t n = data[i] + v.data[i] + carry;
            data[i] = n & 0xffffffff;
            carry = n >> 32;
        }
        return *this;
    }
    inline uint256_t& operator-=(const uint256_t& v) { *this += -v; return *this; }
    uint256_t& operator*=(const uint256_t& v);
    uint256_t& operator/=(const uint256_t& v);
    uint256_t& operator%=(const uint256_t& v);
    inline uint256_t& operator&=(const uint256_t& v) { for (int i = 0; i < W; i++) data[i] &= v.data[i]; return *this; }
    inline uint256_t& operator|=(const uint256_t& v) { for (int i = 0; i < W; i++) data[i] |= v.data[i]; return *this; }
    inline uint256_t& operator^=(const uint256_t& v) { for (int i = 0; i < W; i++) data[i] ^= v.data[i]; return *this; }
    uint256_t& operator>>=(int n);
    uint256_t& operator<<=(int n);
    friend inline const uint256_t operator+(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) += v2; }
    friend inline const uint256_t operator-(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) -= v2; }
    friend inline const uint256_t operator*(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) *= v2; }
    friend inline const uint256_t operator/(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) /= v2; }
    friend inline const uint256_t operator%(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) %= v2; }
    friend inline const uint256_t operator&(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) &= v2; }
    friend inline const uint256_t operator|(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) |= v2; }
    friend inline const uint256_t operator^(const uint256_t& v1, const uint256_t& v2) { return uint256_t(v1) ^= v2; }
    friend inline const uint256_t operator>>(const uint256_t& v, int n) { return uint256_t(v) >>= n; }
    friend inline const uint256_t operator<<(const uint256_t& v, int n) { return uint256_t(v) <<= n; }
    friend inline bool operator==(const uint256_t& v1, const uint256_t& v2) { return v1.cmp(v2) == 0; }
    friend inline bool operator!=(const uint256_t& v1, const uint256_t& v2) { return v1.cmp(v2) != 0; }
    friend inline bool operator<(const uint256_t& v1, const uint256_t& v2) { return v1.cmp(v2) < 0; }
    friend inline bool operator>(const uint256_t& v1, const uint256_t& v2) { return v1.cmp(v2) > 0; }
    friend inline bool operator<=(const uint256_t& v1, const uint256_t& v2) { return v1.cmp(v2) <= 0; }
    friend inline bool operator>=(const uint256_t& v1, const uint256_t& v2) { return v1.cmp(v2) >= 0; }
};

enum Opcode : uint8_t {
    STOP = 0x00,
    ADD = 0x01,
    MUL = 0x02,
    SUB = 0x03,
    DIV = 0x04,
    SDIV = 0x05,
    MOD = 0x06,
    SMOD = 0x07,
    ADDMOD = 0x08,
    MULMOD = 0x09,
    EXP = 0x0a,
    SIGNEXTEND = 0x0b,
    LT = 0x10,
    GT = 0x11,
    SLT = 0x12,
    SGT = 0x13,
    EQ = 0x14,
    ISZERO = 0x15,
    AND = 0x16,
    OR = 0x17,
    XOR = 0x18,
    NOT = 0x19,
    BYTE = 0x1a,
//    SHL = 0x1b,
//    SHR = 0x1c,
//    SAR = 0x1d,
    SHA3 = 0x20,
    ADDRESS = 0x30,
    BALANCE = 0x31,
    ORIGIN = 0x32,
    CALLER = 0x33,
    CALLVALUE = 0x34,
    CALLDATALOAD = 0x35,
    CALLDATASIZE = 0x36,
    CALLDATACOPY = 0x37,
    CODESIZE = 0x38,
    CODECOPY = 0x39,
    GASPRICE = 0x3a,
    EXTCODESIZE = 0x3b,
    EXTCODECOPY = 0x3c,
    RETURNDATASIZE = 0x3d,
    RETURNDATACOPY = 0x3e,
//    EXTCODEHASH = 0x3f,
    BLOCKHASH = 0x40,
    COINBASE = 0x41,
    TIMESTAMP = 0x42,
    NUMBER = 0x43,
    DIFFICULTY = 0x44,
    GASLIMIT = 0x45,
    POP = 0x50,
    MLOAD = 0x51,
    MSTORE = 0x52,
    MSTORE8 = 0x53,
    SLOAD = 0x54,
    SSTORE = 0x55,
    JUMP = 0x56,
    JUMPI = 0x57,
    PC = 0x58,
    MSIZE = 0x59,
    GAS = 0x5a,
    JUMPDEST = 0x5b,
    PUSH1 = 0x60,
    PUSH2 = 0x61,
    PUSH3 = 0x62,
    PUSH4 = 0x63,
    PUSH5 = 0x64,
    PUSH6 = 0x65,
    PUSH7 = 0x66,
    PUSH8 = 0x67,
    PUSH9 = 0x68,
    PUSH10 = 0x69,
    PUSH11 = 0x6a,
    PUSH12 = 0x6b,
    PUSH13 = 0x6c,
    PUSH14 = 0x6d,
    PUSH15 = 0x6e,
    PUSH16 = 0x6f,
    PUSH17 = 0x70,
    PUSH18 = 0x71,
    PUSH19 = 0x72,
    PUSH20 = 0x73,
    PUSH21 = 0x74,
    PUSH22 = 0x75,
    PUSH23 = 0x76,
    PUSH24 = 0x77,
    PUSH25 = 0x78,
    PUSH26 = 0x79,
    PUSH27 = 0x7a,
    PUSH28 = 0x7b,
    PUSH29 = 0x7c,
    PUSH30 = 0x7d,
    PUSH31 = 0x7e,
    PUSH32 = 0x7f,
    DUP1 = 0x80,
    DUP2 = 0x81,
    DUP3 = 0x82,
    DUP4 = 0x83,
    DUP5 = 0x84,
    DUP6 = 0x85,
    DUP7 = 0x86,
    DUP8 = 0x87,
    DUP9 = 0x88,
    DUP10 = 0x89,
    DUP11 = 0x8a,
    DUP12 = 0x8b,
    DUP13 = 0x8c,
    DUP14 = 0x8d,
    DUP15 = 0x8e,
    DUP16 = 0x8f,
    SWAP1 = 0x90,
    SWAP2 = 0x91,
    SWAP3 = 0x92,
    SWAP4 = 0x93,
    SWAP5 = 0x94,
    SWAP6 = 0x95,
    SWAP7 = 0x96,
    SWAP8 = 0x97,
    SWAP9 = 0x98,
    SWAP10 = 0x99,
    SWAP11 = 0x9a,
    SWAP12 = 0x9b,
    SWAP13 = 0x9c,
    SWAP14 = 0x9d,
    SWAP15 = 0x9e,
    SWAP16 = 0x9f,
    LOG0 = 0xa0,
    LOG1 = 0xa1,
    LOG2 = 0xa2,
    LOG3 = 0xa3,
    LOG4 = 0xa4,
    CREATE = 0xf0,
    CALL = 0xf1,
    CALLCODE = 0xf2,
    RETURN = 0xf3,
    DELEGATECALL = 0xf4,
//    CREATE2 = 0xf5,
    STATICCALL = 0xfa,
    REVERT = 0xfd,
    INVALID = 0xfe,
    SELFDESTRUCT = 0xff,
};

class Stack {
private:
    static constexpr int L = 1024;
    uint256_t data[L];
    int top = 0;
public:
    inline const uint256_t pop() { return data[--top]; }
    inline void push(const uint256_t& v) { data[top++] = v; }
    inline uint256_t operator [](int i) const { return data[i < 0 ? top - i: i]; }
    inline uint256_t& operator [](int i) { return data[i < 0 ? top - i: i]; }
};

uint256_t get_balance(uint256_t address)
{
    return 0;
}

uint256_t code_size_at(uint256_t address)
{
    return 0;
}

void vm_step()
{
    uint256_t gas_price;
    uint256_t gas_limit;
    uint256_t caller_address;
    uint256_t origin_address;
    uint256_t owner_address;
    uint32_t code_size;
    uint8_t code[1];
    uint32_t pc = 0;
    Stack stack;
    for (;;) {
        uint8_t opc = code[pc];
        switch (opc) {
        case STOP: { /* implement */ break; }
        case ADD: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 + v2); pc++; break; }
        case MUL: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 * v2); pc++; break; }
        case SUB: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 - v2); pc++; break; }
        case DIV: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 / v2); pc++; break; }
        case SDIV: { /* implement */ break; }
        case MOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 % v2); pc++; break; }
        case SMOD: { /* implement */ break; }
        case ADDMOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(); stack.push((v1 + v2) % v3); pc++; break; }
        case MULMOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(); stack.push((v1 * v2) % v3); pc++; break; }
        case EXP: { /* implement */ break; }
        case SIGNEXTEND: { /* implement */ break; }
        case LT: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = v1 < v2; stack.push(v3); pc++; break; }
        case GT: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = v1 > v2; stack.push(v3); pc++; break; }
        case SLT: { /* implement */ break; }
        case SGT: { /* implement */ break; }
        case EQ: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = v1 == v2; stack.push(v3); pc++; break; }
        case ISZERO: { uint256_t v1 = stack.pop(), v2 = v1 == 0; stack.push(v2); pc++; break; }
        case AND: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = v1 & v2; stack.push(v3); pc++; break; }
        case OR: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = v1 | v2; stack.push(v3); pc++; break; }
        case XOR: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = v1 ^ v2; stack.push(v3); pc++; break; }
        case NOT: { uint256_t v1 = stack.pop(), v2 = ~v1; stack.push(v2); pc++; break; }
        case BYTE: { /* implement */ break; }
        case SHA3: { /* implement */ break; }
        case ADDRESS: {
            uint256_t v1 = owner_address;
            stack.push(v1);
            pc++;
            break;
        }
        case BALANCE: {
            uint256_t v1 = stack.pop();
            uint256_t v2 = get_balance(v1);
            stack.push(v2);
            pc++;
            break;
        }
        case ORIGIN: {
            uint256_t v1 = origin_address;
            stack.push(v1);
            pc++;
            break;
        }
        case CALLER: {
            uint256_t v1 = caller_address;
            stack.push(v1);
            pc++;
            break;
        }
        case CALLVALUE: { /* implement */ break; }
        case CALLDATALOAD: { /* implement */ break; }
        case CALLDATASIZE: { /* implement */ break; }
        case CALLDATACOPY: { /* implement */ break; }
        case CODESIZE: {
            uint256_t v1 = code_size;
            stack.push(v1);
            pc++;
            break;
        }
        case CODECOPY: { /* implement */ break; }
        case GASPRICE: {
            uint256_t v1 = gas_price;
            stack.push(v1);
            pc++;
            break;
        }
        case EXTCODESIZE: {
            uint256_t v1 = stack.pop();
            uint256_t v2 = code_size_at(v1);
            stack.push(v2);
            pc++;
            break;
        }
        case EXTCODECOPY: { /* implement */ break; }
        case RETURNDATASIZE: { /* implement */ break; }
        case RETURNDATACOPY: { /* implement */ break; }
        case BLOCKHASH: { /* implement */ break; }
        case COINBASE: { /* implement */ break; }
        case TIMESTAMP: { /* implement */ break; }
        case NUMBER: { /* implement */ break; }
        case DIFFICULTY: { /* implement */ break; }
        case GASLIMIT: {
            uint256_t v1 = gas_limit;
            stack.push(v1);
            pc++;
            break;
        }
        case POP: { stack.pop(); pc++; break; }
        case MLOAD: {
            uint256_t v1 = stack.pop();
            uint256_t v2 = /*mem.load(v1)*/0;
            stack.push(v2);
            pc++;
            break;
        }
        case MSTORE: { /* implement */ break; }
        case MSTORE8: { /* implement */ break; }
        case SLOAD: { /* implement */ break; }
        case SSTORE: { /* implement */ break; }
        case JUMP: { /* implement */ break; }
        case JUMPI: { /* implement */ break; }
        case PC: { /* implement */ break; }
        case MSIZE: { /* implement */ break; }
        case GAS: { /* implement */ break; }
        case JUMPDEST: { /* implement */ break; }
        case PUSH1: { stack.push(1); pc++; break; }
        case PUSH2: { stack.push(2); pc++; break; }
        case PUSH3: { stack.push(3); pc++; break; }
        case PUSH4: { stack.push(4); pc++; break; }
        case PUSH5: { stack.push(5); pc++; break; }
        case PUSH6: { stack.push(6); pc++; break; }
        case PUSH7: { stack.push(7); pc++; break; }
        case PUSH8: { stack.push(8); pc++; break; }
        case PUSH9: { stack.push(9); pc++; break; }
        case PUSH10: { stack.push(10); pc++; break; }
        case PUSH11: { stack.push(11); pc++; break; }
        case PUSH12: { stack.push(12); pc++; break; }
        case PUSH13: { stack.push(13); pc++; break; }
        case PUSH14: { stack.push(14); pc++; break; }
        case PUSH15: { stack.push(15); pc++; break; }
        case PUSH16: { stack.push(16); pc++; break; }
        case PUSH17: { stack.push(17); pc++; break; }
        case PUSH18: { stack.push(18); pc++; break; }
        case PUSH19: { stack.push(19); pc++; break; }
        case PUSH20: { stack.push(20); pc++; break; }
        case PUSH21: { stack.push(21); pc++; break; }
        case PUSH22: { stack.push(22); pc++; break; }
        case PUSH23: { stack.push(23); pc++; break; }
        case PUSH24: { stack.push(24); pc++; break; }
        case PUSH25: { stack.push(25); pc++; break; }
        case PUSH26: { stack.push(26); pc++; break; }
        case PUSH27: { stack.push(27); pc++; break; }
        case PUSH28: { stack.push(28); pc++; break; }
        case PUSH29: { stack.push(29); pc++; break; }
        case PUSH30: { stack.push(30); pc++; break; }
        case PUSH31: { stack.push(31); pc++; break; }
        case PUSH32: { stack.push(32); pc++; break; }
        case DUP1: { uint256_t v1 = stack[-1]; stack.push(v1); pc++; break; }
        case DUP2: { uint256_t v1 = stack[-2]; stack.push(v1); pc++; break; }
        case DUP3: { uint256_t v1 = stack[-3]; stack.push(v1); pc++; break; }
        case DUP4: { uint256_t v1 = stack[-4]; stack.push(v1); pc++; break; }
        case DUP5: { uint256_t v1 = stack[-5]; stack.push(v1); pc++; break; }
        case DUP6: { uint256_t v1 = stack[-6]; stack.push(v1); pc++; break; }
        case DUP7: { uint256_t v1 = stack[-7]; stack.push(v1); pc++; break; }
        case DUP8: { uint256_t v1 = stack[-8]; stack.push(v1); pc++; break; }
        case DUP9: { uint256_t v1 = stack[-9]; stack.push(v1); pc++; break; }
        case DUP10: { uint256_t v1 = stack[-10]; stack.push(v1); pc++; break; }
        case DUP11: { uint256_t v1 = stack[-11]; stack.push(v1); pc++; break; }
        case DUP12: { uint256_t v1 = stack[-12]; stack.push(v1); pc++; break; }
        case DUP13: { uint256_t v1 = stack[-13]; stack.push(v1); pc++; break; }
        case DUP14: { uint256_t v1 = stack[-14]; stack.push(v1); pc++; break; }
        case DUP15: { uint256_t v1 = stack[-15]; stack.push(v1); pc++; break; }
        case DUP16: { uint256_t v1 = stack[-16]; stack.push(v1); pc++; break; }
        case SWAP1: { uint256_t v1 = stack[-1]; stack[-1] = stack[-2]; stack[-2] = v1; pc++; break; }
        case SWAP2: { uint256_t v1 = stack[-1]; stack[-1] = stack[-3]; stack[-3] = v1; pc++; break; }
        case SWAP3: { uint256_t v1 = stack[-1]; stack[-1] = stack[-4]; stack[-4] = v1; pc++; break; }
        case SWAP4: { uint256_t v1 = stack[-1]; stack[-1] = stack[-5]; stack[-5] = v1; pc++; break; }
        case SWAP5: { uint256_t v1 = stack[-1]; stack[-1] = stack[-6]; stack[-6] = v1; pc++; break; }
        case SWAP6: { uint256_t v1 = stack[-1]; stack[-1] = stack[-7]; stack[-7] = v1; pc++; break; }
        case SWAP7: { uint256_t v1 = stack[-1]; stack[-1] = stack[-8]; stack[-8] = v1; pc++; break; }
        case SWAP8: { uint256_t v1 = stack[-1]; stack[-1] = stack[-9]; stack[-9] = v1; pc++; break; }
        case SWAP9: { uint256_t v1 = stack[-1]; stack[-1] = stack[-10]; stack[-10] = v1; pc++; break; }
        case SWAP10: { uint256_t v1 = stack[-1]; stack[-1] = stack[-11]; stack[-11] = v1; pc++; break; }
        case SWAP11: { uint256_t v1 = stack[-1]; stack[-1] = stack[-12]; stack[-12] = v1; pc++; break; }
        case SWAP12: { uint256_t v1 = stack[-1]; stack[-1] = stack[-13]; stack[-13] = v1; pc++; break; }
        case SWAP13: { uint256_t v1 = stack[-1]; stack[-1] = stack[-14]; stack[-14] = v1; pc++; break; }
        case SWAP14: { uint256_t v1 = stack[-1]; stack[-1] = stack[-15]; stack[-15] = v1; pc++; break; }
        case SWAP15: { uint256_t v1 = stack[-1]; stack[-1] = stack[-16]; stack[-16] = v1; pc++; break; }
        case SWAP16: { uint256_t v1 = stack[-1]; stack[-1] = stack[-17]; stack[-17] = v1; pc++; break; }
        case LOG0: { /* implement */ break; }
        case LOG1: { /* implement */ break; }
        case LOG2: { /* implement */ break; }
        case LOG3: { /* implement */ break; }
        case LOG4: { /* implement */ break; }
        case CREATE: { /* implement */ break; }
        case CALL: { /* implement */ break; }
        case CALLCODE: { /* implement */ break; }
        case RETURN: { /* implement */ break; }
        case DELEGATECALL: { /* implement */ break; }
        case STATICCALL: { /* implement */ break; }
        case REVERT: { /* implement */ break; }
        case INVALID: { /* implement */ break; }
        case SELFDESTRUCT: { /* implement */ break; }
        default:
            // error invalid opcode
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    return 0;
}
