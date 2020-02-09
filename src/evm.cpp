#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdint.h>

/* error */

enum Error {
    MEMORY_EXAUSTED = 1,
    ILLEGAL_TARGET,
    ILLEGAL_UPDATE,
    INVALID_ENCODING,
    INVALID_OPCODE,
    INVALID_SIZE,
    INVALID_TRANSACTION,
    OUTOFBOUND_INDEX,
    STACK_OVERFLOW,
    STACK_UNDERFLOW,
    UNIMPLEMENTED,
};

static const char *errors[UNIMPLEMENTED+1] = {
    nullptr,
    "MEMORY_EXAUSTED",
    "ILLEGAL_TARGET",
    "ILLEGAL_UPDATE",
    "INVALID_ENCODING",
    "INVALID_OPCODE",
    "INVALID_SIZE",
    "INVALID_TRANSACTION",
    "OUTOFBOUND_INDEX",
    "STACK_OVERFLOW",
    "STACK_UNDERFLOW",
    "UNIMPLEMENTED",
};

/* uintX_t */

const uint32_t _WORD = 0xdeadbeef;
const bool BIGENDIAN = ((const uint8_t*)&_WORD)[0] == 0xde;

template<int X>
class uintX_t {
private:
    template<int Y> friend class uintX_t;
    static constexpr int B = X / 8;
    static constexpr int W = X / 32;
    uint32_t data[W];
    inline int cmp(const uintX_t& v) const {
        for (int i = 0; i < W; i++) {
            if (data[i] < v.data[i]) return -1;
            if (data[i] > v.data[i]) return 1;
        }
        return 0;
    }
public:
    inline uintX_t() {}
    inline uintX_t(uint32_t v) { data[0] = v; for (int i = 1; i < W; i++) data[i] = 0; }
    template<int Y> inline uintX_t(const uintX_t<Y> &v) {
        int s = v.W < W ? v.W : W;
        for (int i = 0; i < s; i++) data[i] = v.data[i];
        for (int i = s; i < W; i++) data[i] = 0;
    }
    inline uintX_t& operator=(const uintX_t& v) { for (int i = 0; i < W; i++) data[i] = v.data[i]; return *this; }
    inline uint64_t cast32() const { return data[0]; }
    inline const uintX_t sigflip() const { uintX_t v = *this; v.data[W-1] ^= 0x80000000; return v; }
    inline const uintX_t operator~() const { uintX_t v; for (int i = 0; i < W; i++) v.data[i] = ~data[i]; return v; }
    inline const uintX_t operator-() const { uintX_t v = ~(*this); return ++v; }
    inline uintX_t& operator++() { for (int i = 0; i < W; i++) if (++data[i] != 0) break; return *this; }
    inline uintX_t& operator--() { for (int i = 0; i < W; i++) if (data[i]-- != 0) break; return *this; }
    inline const uintX_t operator++(int) { const uintX_t v = *this; ++(*this); return v; }
    inline const uintX_t operator--(int) { const uintX_t v = *this; --(*this); return v; }
    inline uintX_t& operator+=(const uintX_t& v)
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
    inline uintX_t& operator-=(const uintX_t& v) { *this += -v; return *this; }
    inline uintX_t& operator*=(const uintX_t& v) {
        uintX_t t = *this;
        *this = 0;
        for (int j = 0; j < W; j++) {
            uint64_t base = v.data[j];
            uint64_t carry = 0;
            for (int i = j; i < W; i++) {
                uint64_t n = data[i] + base * t.data[i-j] + carry;
                data[i] = n & 0xffffffff;
                carry = n >> 32;
            }
        }
        return *this;
    }
    inline uintX_t& operator/=(const uintX_t& v) { throw UNIMPLEMENTED; }
    inline uintX_t& operator%=(const uintX_t& v) { *this -= (*this / v) * v; return *this; }
    inline uintX_t& operator&=(const uintX_t& v) { for (int i = 0; i < W; i++) data[i] &= v.data[i]; return *this; }
    inline uintX_t& operator|=(const uintX_t& v) { for (int i = 0; i < W; i++) data[i] |= v.data[i]; return *this; }
    inline uintX_t& operator^=(const uintX_t& v) { for (int i = 0; i < W; i++) data[i] ^= v.data[i]; return *this; }
    inline uintX_t& operator<<=(int n) {
        if (n < 0 || n >= 8*B) throw OUTOFBOUND_INDEX;
        if (n == 0) return *this;
        int index = n / 32;
        int shift = n % 32;
        for (int i = W - 1; i >= 0; i--) {
            uint32_t w = 0;
            if (i >= index) w |= data[i - index] << shift;
            if (i >= index + 1 && shift > 0) w |= data[i - index - 1] >> (32 - shift);
            data[i] = w;
        }
        return *this;
    }
    inline uintX_t& operator>>=(int n) {
        if (n < 0 || n >= 8*B) throw OUTOFBOUND_INDEX;
        if (n == 0) return *this;
        int index = n / 32;
        int shift = n % 32;
        for (int i = 0; i < W; i++) {
            uint32_t w = 0;
            if (W > i + index) w |= data[i + index] >> shift;
            if (W > i + index + 1 && shift > 0) w |= data[i + index + 1] << (32 - shift);
            data[i] = w;
        }
        return *this;
    }
    inline const uintX_t sigext(int n) const {
        if (n < 0 || n >= B) throw OUTOFBOUND_INDEX;
        int shift = 8 * n;
        uintX_t t = *this << shift;
        return sar(t, shift);
    }
    friend inline const uintX_t operator+(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) += v2; }
    friend inline const uintX_t operator-(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) -= v2; }
    friend inline const uintX_t operator*(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) *= v2; }
    friend inline const uintX_t operator/(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) /= v2; }
    friend inline const uintX_t operator%(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) %= v2; }
    friend inline const uintX_t operator&(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) &= v2; }
    friend inline const uintX_t operator|(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) |= v2; }
    friend inline const uintX_t operator^(const uintX_t& v1, const uintX_t& v2) { return uintX_t(v1) ^= v2; }
    friend inline const uintX_t operator<<(const uintX_t& v, int n) { return uintX_t(v) <<= n; }
    friend inline const uintX_t operator>>(const uintX_t& v, int n) { return uintX_t(v) >>= n; }
    friend inline bool operator==(const uintX_t& v1, const uintX_t& v2) { return v1.cmp(v2) == 0; }
    friend inline bool operator!=(const uintX_t& v1, const uintX_t& v2) { return v1.cmp(v2) != 0; }
    friend inline bool operator<(const uintX_t& v1, const uintX_t& v2) { return v1.cmp(v2) < 0; }
    friend inline bool operator>(const uintX_t& v1, const uintX_t& v2) { return v1.cmp(v2) > 0; }
    friend inline bool operator<=(const uintX_t& v1, const uintX_t& v2) { return v1.cmp(v2) <= 0; }
    friend inline bool operator>=(const uintX_t& v1, const uintX_t& v2) { return v1.cmp(v2) >= 0; }
    inline uint8_t operator[](int n) const {
        if (n < 0 || n >= B) throw OUTOFBOUND_INDEX;
        n = B - 1 - n;
        int i = n / 4;
        int j = n % 4;
        int shift = 8*j;
        return (uint8_t)(data[i] >> shift);
    }
    inline uint8_t& operator[](int n) {
        if (n < 0 || n >= B) throw OUTOFBOUND_INDEX;
        n = B - 1 - n;
        int i = n / 4;
        int j = n % 4;
        if (BIGENDIAN) j = 3 - j;
        return ((uint8_t*)&data[i])[j];
    }
    static inline const uintX_t sar(const uintX_t &v, int n) {
        if (n < 0 || n >= 8*B) throw OUTOFBOUND_INDEX;
        if (n == 0) return v;
        uintX_t t = v;
        bool is_neg = (t[0] & 0x80) > 0;
        if (is_neg) t = ~t;
        t >>= n;
        if (is_neg) t = ~t;
        return t;
    }
    static inline const uintX_t pow(const uintX_t &v1, const uintX_t &v2) {
        uintX_t x1 = 1;
        uintX_t x2 = v1;
        for (int n = 8*B - 1; n >= 0; n--) {
            int i = n / 32;
            int j = n % 32;
            uintX_t t = (v2.data[i] & (1 << j)) == 0 ? x1 : x2;
            x1 *= t;
            x2 *= t;
        }
        return x1;
    }
    static inline const uintX_t addmod(const uintX_t &v1, const uintX_t &v2, const uintX_t &v3) {
        uintX_t<B+32> _v1 = v1;
        uintX_t<B+32> _v2 = v2;
        uintX_t<B+32> _v3 = v3;
        return (_v1 + _v2) % _v3;
    }
    static inline const uintX_t mulmod(const uintX_t &v1, const uintX_t &v2, const uintX_t &v3) {
        uintX_t<2*B> _v1 = v1;
        uintX_t<2*B> _v2 = v2;
        uintX_t<2*B> _v3 = v3;
        return (_v1 * _v2) % _v3;
    }
    static inline const uintX_t from(const char *buffer) { return from((const uint8_t*)buffer); }
    static inline const uintX_t from(const uint8_t *buffer) { return from(buffer, B); }
    static inline const uintX_t from(const uint8_t *buffer, int size) {
        if (size < 0 || size > B) throw INVALID_SIZE;
        uintX_t v = 0;
        for (int j = 0; j < size; j++) {
            int i = j + B - size;
            v[i] = buffer[j];
        }
        return v;
    }
    friend std::ostream& operator<<(std::ostream &os, const uintX_t &v) {
        for (int i = 0; i < B; i++) {
            os << std::hex << std::setw(2) << std::setfill('0') << (uint32_t)v[i];
        }
        return os;
    }
};

class uint160_t : public uintX_t<160> {
public:
    inline uint160_t() {}
    inline uint160_t(uint32_t v) : uintX_t(v) {}
    inline uint160_t(const uintX_t& v) : uintX_t(v) {}
};

class uint256_t : public uintX_t<256> {
public:
    inline uint256_t() {}
    inline uint256_t(uint32_t v) : uintX_t(v) {}
    inline uint256_t(const uintX_t& v) : uintX_t(v) {}
};

class uint512_t : public uintX_t<512> {
public:
    inline uint512_t() {}
    inline uint512_t(uint32_t v) : uintX_t(v) {}
    inline uint512_t(const uintX_t& v) : uintX_t(v) {}
};

static inline void word(const uint256_t &v, uint8_t *data, int size)
{
    for (int i = 0; i < size; i++) {
        data[size - i - 1] = (v >> 8 * i).cast32() & 0xff;
    }
}

/* crypto */

static inline uint64_t rot(uint64_t x, int y)
{
    return y > 0 ? (x >> (64 - y)) ^ (x << y) : x;
}

static inline uint64_t b2w(const uint8_t *b)
{
    return 0
        | ((uint64_t)b[7] << 56)
        | ((uint64_t)b[6] << 48)
        | ((uint64_t)b[5] << 40)
        | ((uint64_t)b[4] << 32)
        | ((uint64_t)b[3] << 24)
        | ((uint64_t)b[2] << 16)
        | ((uint64_t)b[1] << 8)
        | (uint64_t)b[0];
}

static inline void w2b(uint64_t w, uint8_t *b)
{
    b[0] = (uint8_t)w;
    b[1] = (uint8_t)(w >> 8);
    b[2] = (uint8_t)(w >> 16);
    b[3] = (uint8_t)(w >> 24);
    b[4] = (uint8_t)(w >> 32);
    b[5] = (uint8_t)(w >> 40);
    b[6] = (uint8_t)(w >> 48);
    b[7] = (uint8_t)(w >> 56);
}

static void sha3(const uint8_t *message, uint32_t size, bool compressed, uint32_t r, uint8_t eof, uint8_t *output)
{
    if (!compressed) {
        uint32_t bitsize = 8 * size;
        uint32_t padding = (r - bitsize % r) / 8;
        uint32_t b_len = size + padding;
        uint8_t b[b_len];
        for (uint32_t i = 0; i < size; i++) b[i] = message[i];
        for (uint32_t i = size; i < b_len; i++) b[i] = 0;
        b[size] |= eof;
        b[b_len-1] |= 0x80;
        sha3(b, b_len, true, r, eof, output);
        return;
    }
    const uint64_t RC[24] = {
        0x0000000000000001L, 0x0000000000008082L, 0x800000000000808aL,
        0x8000000080008000L, 0x000000000000808bL, 0x0000000080000001L,
        0x8000000080008081L, 0x8000000000008009L, 0x000000000000008aL,
        0x0000000000000088L, 0x0000000080008009L, 0x000000008000000aL,
        0x000000008000808bL, 0x800000000000008bL, 0x8000000000008089L,
        0x8000000000008003L, 0x8000000000008002L, 0x8000000000000080L,
        0x000000000000800aL, 0x800000008000000aL, 0x8000000080008081L,
        0x8000000000008080L, 0x0000000080000001L, 0x8000000080008008L,
    };
    const uint8_t R[5][5] = {
        { 0, 36,  3, 41, 18},
        { 1, 44, 10, 45,  2},
        {62,  6, 43, 15, 61},
        {28, 55, 25, 21, 56},
        {27, 20, 39,  8, 14},
    };
    uint64_t s[5][5];
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            s[x][y] = 0;
        }
    }
    uint32_t k = r / 64;
    for (uint32_t j = 0; j < size/8; j += k) {
        uint64_t w[25];
        for (uint32_t i = 0; i < k; i++) {
            w[i] = b2w(&message[8*(j+i)]);
        }
        for (int i = k; i < 25; i++) {
            w[i] = 0;
        }
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                s[x][y] ^= w[5 * y + x];
            }
        }
        for (int j = 0; j < 24; j++) {
            uint64_t c[5];
            for (int x = 0; x < 5; x++) {
                c[x] = s[x][0] ^ s[x][1] ^ s[x][2] ^ s[x][3] ^ s[x][4];
            }
            uint64_t d[5];
            for (int x = 0; x < 5; x++) {
                d[x] = c[(x + 4) % 5] ^ rot(c[(x + 1) % 5], 1);
            }
            for (int x = 0; x < 5; x++) {
                for (int y = 0; y < 5; y++) {
                    s[x][y] ^= d[x];
                }
            }
            uint64_t b[5][5];
            for (int x = 0; x < 5; x++) {
                for (int y = 0; y < 5; y++) {
					b[y][(2 * x + 3 * y) % 5] = rot(s[x][y], R[x][y]);
                }
            }
            for (int x = 0; x < 5; x++) {
                for (int y = 0; y < 5; y++) {
                    s[x][y] = b[x][y] ^ (~b[(x + 1) % 5][y] & b[(x + 2) % 5][y]);
                }
            }
            s[0][0] ^= RC[j];
        }
    }
    w2b(s[0][0], &output[0]);
    w2b(s[1][0], &output[8]);
    w2b(s[2][0], &output[16]);
    w2b(s[3][0], &output[24]);
    w2b(s[4][0], &output[32]);
    w2b(s[0][1], &output[40]);
    w2b(s[1][1], &output[48]);
    w2b(s[2][1], &output[56]);
}

static uint256_t sha3(const uint8_t *buffer, uint32_t size)
{
    uint8_t output[64];
    sha3(buffer, size, false, 1088, 0x01, output);
    return uint256_t::from(output);
}

/* decoder */

struct txn {
    uint256_t nonce;
    uint256_t gasprice;
    uint256_t gaslimit;
    bool has_to;
    uint256_t to;
    uint256_t value;
    uint8_t *data;
    uint32_t data_size;
    bool is_signed;
    uint256_t v;
    uint256_t r;
    uint256_t s;
};

uint256_t parse_nlzint(const uint8_t *&b, uint32_t &s, uint32_t l)
{
    if (l == 0) return 0;
    if (s < l) throw INVALID_ENCODING;
    if (b[0] == 0) throw INVALID_ENCODING;
    if (l > 32) throw INVALID_ENCODING;
    uint256_t v = uint256_t::from(b, l);
    b += l; s -= l;
    return v;
}

uint256_t parse_nlzint(const uint8_t *b, uint32_t s)
{
    return parse_nlzint(b, s, s);
}

uint32_t parse_varlen(const uint8_t *&b, uint32_t &s, bool &is_list)
{
	if (s < 1) throw INVALID_ENCODING;
    uint8_t n = b[0];
    if (n < 0x80) { is_list = false; return 1; }
    b++; s--;
	if (n >= 0xc0 + 56) {
		uint32_t l = parse_nlzint(b, s, n - (0xc0 + 56) + 1).cast32();
		if (l < 56) throw INVALID_ENCODING;
        is_list = true; return l;
    }
	if (n >= 0xc0) { is_list = true; return n - 0xc0; }
	if (n >= 0x80 + 56) {
		uint32_t l = parse_nlzint(b, s, n - (0x80 + 56) + 1).cast32();
		if (l < 56) throw INVALID_ENCODING;
        is_list = true; return l;
    }
	if (n == 0x81) {
        if (s < 1) throw INVALID_ENCODING;
        uint8_t n = b[0];
        if (n < 0x80) throw INVALID_ENCODING;
    }
    is_list = false; return n - 0x80;
}

struct rlp {
    bool is_list;
    uint32_t size;
    union {
        uint8_t *data;
        struct rlp *list;
    };
};

void free_rlp(struct rlp &rlp)
{
    if (rlp.is_list) {
        for (uint32_t i = 0; i < rlp.size; i++) free_rlp(rlp.list[i]);
        delete rlp.list;
        rlp.size = 0;
        rlp.list = nullptr;
    } else {
        delete rlp.data;
        rlp.size = 0;
        rlp.data = nullptr;
    }
}

void parse_rlp(const uint8_t *&b, uint32_t &s, struct rlp &rlp)
{
    bool is_list;
	uint32_t l = parse_varlen(b, s, is_list);
	if (l > s) throw INVALID_ENCODING;
    const uint8_t *_b = b;
    uint32_t _s = l;
    b += l; s -= l;
    if (is_list) {
        uint32_t size = 0;
        struct rlp *list = nullptr;
        while (_s > 0) {
            try {
                struct rlp *new_list = new struct rlp[size + 1];
                if (new_list == nullptr) throw MEMORY_EXAUSTED;
                for (uint32_t i = 0; i < size; i++) new_list[i] = list[i];
                delete list;
                list = new_list;
                parse_rlp(_b, _s, list[size]);
            } catch (Error e) {
                for (uint32_t i = 0; i < size; i++) free_rlp(list[i]);
                delete list;
                throw e;
            }
            size++;
        }
        rlp.is_list = is_list;
        rlp.size = size;
        rlp.list = list;
    } else {
        uint32_t size = _s;
        uint8_t *data = new uint8_t[size];
        if (data == nullptr) throw MEMORY_EXAUSTED;
        for (uint32_t i = 0; i < size; i++) data[i] = _b[i];
        rlp.is_list = is_list;
        rlp.size = size;
        rlp.data = data;
    }
}

struct txn decode_txn(const uint8_t *buffer, uint32_t size)
{
    struct rlp rlp;
    parse_rlp(buffer, size, rlp);
    struct txn txn = { 0, 0, 0, false, 0, 0, nullptr, 0, false, 0, 0, 0 };
    try {
        if (size > 0) throw INVALID_TRANSACTION;
        if (rlp.size != 6 && rlp.size != 9) throw INVALID_TRANSACTION;
        if (!rlp.is_list) throw INVALID_TRANSACTION;
        for (uint32_t i = 0; i < rlp.size; i++) {
            if (rlp.list[i].is_list) throw INVALID_TRANSACTION;
        }
        txn.nonce = parse_nlzint(rlp.list[0].data, rlp.list[0].size);
        txn.gasprice = parse_nlzint(rlp.list[1].data, rlp.list[1].size);
        txn.gaslimit = parse_nlzint(rlp.list[2].data, rlp.list[2].size);
        txn.has_to = rlp.list[3].size > 0;
        if (txn.has_to) {
            if (rlp.list[3].size != 20) throw INVALID_TRANSACTION;
            txn.to = uint256_t::from(rlp.list[3].data, rlp.list[3].size);
        }
        txn.value = parse_nlzint(rlp.list[4].data, rlp.list[4].size);
        txn.data_size = rlp.list[5].size;
        txn.data = new uint8_t[txn.data_size];
        if (txn.data == nullptr) throw MEMORY_EXAUSTED;
        for (uint32_t i = 0; i < txn.data_size; i++) txn.data[i] = rlp.list[5].data[i];
        txn.is_signed = rlp.size > 6;
        if (txn.is_signed) {
            txn.v = parse_nlzint(rlp.list[6].data, rlp.list[6].size);
            txn.r = parse_nlzint(rlp.list[7].data, rlp.list[7].size);
            txn.s = parse_nlzint(rlp.list[8].data, rlp.list[8].size);
        }
    } catch (Error e) {
        delete txn.data;
        free_rlp(rlp);
        throw e;
    }
    free_rlp(rlp);
    return txn;
}

/* interpreter */

enum Opcode : uint8_t {
    STOP = 0x00, ADD, MUL, SUB, DIV, SDIV, MOD, SMOD,
    ADDMOD, MULMOD, EXP, SIGNEXTEND,
    // 0x0c .. 0x0f
    LT = 0x10, GT, SLT, SGT, EQ, ISZERO, AND, OR,
    XOR, NOT, BYTE, SHL, SHR, SAR,
    // 0x1e .. 0x1f
    SHA3 = 0x20,
    // 0x21 .. 0x2f
    ADDRESS = 0x30, BALANCE, ORIGIN, CALLER, CALLVALUE, CALLDATALOAD, CALLDATASIZE, CALLDATACOPY,
    CODESIZE, CODECOPY, GASPRICE, EXTCODESIZE, EXTCODECOPY, RETURNDATASIZE, RETURNDATACOPY, EXTCODEHASH,
    BLOCKHASH, COINBASE, TIMESTAMP, NUMBER, DIFFICULTY, GASLIMIT, CHAINID, SELFBALANCE,
    // 0x48 .. 0x4f
    POP = 0x50, MLOAD, MSTORE, MSTORE8, SLOAD, SSTORE, JUMP, JUMPI,
    PC, MSIZE, GAS, JUMPDEST,
    // 0x5c .. 0x5f
    PUSH1 = 0x60, PUSH2, PUSH3, PUSH4, PUSH5, PUSH6, PUSH7, PUSH8,
    PUSH9, PUSH10, PUSH11, PUSH12, PUSH13, PUSH14, PUSH15, PUSH16,
    PUSH17, PUSH18, PUSH19, PUSH20, PUSH21, PUSH22, PUSH23, PUSH24,
    PUSH25, PUSH26, PUSH27, PUSH28, PUSH29, PUSH30, PUSH31, PUSH32,
    DUP1, DUP2, DUP3, DUP4, DUP5, DUP6, DUP7, DUP8,
    DUP9, DUP10, DUP11, DUP12, DUP13, DUP14, DUP15, DUP16,
    SWAP1, SWAP2, SWAP3, SWAP4, SWAP5, SWAP6, SWAP7, SWAP8,
    SWAP9, SWAP10, SWAP11, SWAP12, SWAP13, SWAP14, SWAP15, SWAP16,
    LOG0, LOG1, LOG2, LOG3, LOG4,
    // 0xa5 .. 0xef
    CREATE = 0xf0, CALL, CALLCODE, RETURN, DELEGATECALL, CREATE2,
    // 0xf6 .. 0xf9
    STATICCALL = 0xfa,
    // 0xfb .. 0xfc
    REVERT = 0xfd,
    // 0xfe .. 0xfe
    SELFDESTRUCT = 0xff,
};

static const char *opcodes[256] = {
    "STOP", "ADD", "MUL", "SUB", "DIV", "SDIV", "MOD", "SMOD",
    "ADDMOD", "MULMOD", "EXP", "SIGNEXTEND", "?", "?", "?", "?",
    "LT", "GT", "SLT", "SGT", "EQ", "ISZERO", "AND", "OR",
    "XOR", "NOT", "BYTE", "SHL", "SHR", "SAR", "?", "?",
    "SHA3", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "ADDRESS", "BALANCE", "ORIGIN", "CALLER", "CALLVALUE", "CALLDATALOAD", "CALLDATASIZE", "CALLDATACOPY",
    "CODESIZE", "CODECOPY", "GASPRICE", "EXTCODESIZE", "EXTCODECOPY", "RETURNDATASIZE", "RETURNDATACOPY", "EXTCODEHASH",
    "BLOCKHASH", "COINBASE", "TIMESTAMP", "NUMBER", "DIFFICULTY", "GASLIMIT", "CHAINID", "SELFBALANCE",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "POP", "MLOAD", "MSTORE", "MSTORE8", "SLOAD", "SSTORE", "JUMP", "JUMPI",
    "PC", "MSIZE", "GAS", "JUMPDEST", "?", "?", "?", "?",
    "PUSH1", "PUSH2", "PUSH3", "PUSH4", "PUSH5", "PUSH6", "PUSH7", "PUSH8",
    "PUSH9", "PUSH10", "PUSH11", "PUSH12", "PUSH13", "PUSH14", "PUSH15", "PUSH16",
    "PUSH17", "PUSH18", "PUSH19", "PUSH20", "PUSH21", "PUSH22", "PUSH23", "PUSH24",
    "PUSH25", "PUSH26", "PUSH27", "PUSH28", "PUSH29", "PUSH30", "PUSH31", "PUSH32",
    "DUP1", "DUP2", "DUP3", "DUP4", "DUP5", "DUP6", "DUP7", "DUP8",
    "DUP9", "DUP10", "DUP11", "DUP12", "DUP13", "DUP14", "DUP15", "DUP16",
    "SWAP1", "SWAP2", "SWAP3", "SWAP4", "SWAP5", "SWAP6", "SWAP7", "SWAP8",
    "SWAP9", "SWAP10", "SWAP11", "SWAP12", "SWAP13", "SWAP14", "SWAP15", "SWAP16",
    "LOG0", "LOG1", "LOG2", "LOG3", "LOG4", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
    "CREATE", "CALL", "CALLCODE", "RETURN", "DELEGATECALL", "CREATE2", "?", "?",
    "?", "?", "STATICCALL", "?", "?", "REVERT", "?", "SELFDESTRUCT",
};

class Stack {
private:
    static constexpr int L = 1024;
    uint256_t data[L];
    int top = 0;
public:
    inline const uint256_t pop() { if (top == 0) throw STACK_UNDERFLOW;  return data[--top]; }
    inline void push(const uint256_t& v) { if (top == L) throw STACK_OVERFLOW; data[top++] = v; }
    inline uint256_t operator[](int i) const { return data[i < 0 ? top - i: i]; }
    inline uint256_t& operator[](int i) { return data[i < 0 ? top - i: i]; }
};

class Memory {
private:
    static constexpr int P = 4096;
    static constexpr int S = P / sizeof(uint8_t*);
    uint32_t limit = 0;
    uint32_t page_count = 0;
    uint8_t **pages;
public:
    ~Memory() {
        for (uint32_t i = 0; i < page_count; i++) delete pages[i];
        delete pages;
    }
    inline uint32_t size() const { return limit; }
    inline uint8_t operator[](uint32_t i) const {
        uint32_t page_index = i / P;
        uint32_t byte_index = i % P;
        if (page_index >= page_count) return 0;
        if (pages[page_index] == nullptr) return 0;
        return pages[page_index][byte_index];
    }
    inline uint8_t& operator[](uint32_t i) {
        uint32_t page_index = i / P;
        uint32_t byte_index = i % P;
        if (page_index >= page_count) {
            uint32_t new_page_count = ((page_index / S) + 1) * S;
            uint8_t **new_pages = new uint8_t*[new_page_count];
            if (new_pages == nullptr) throw MEMORY_EXAUSTED;
            for (uint32_t i = 0; i < page_count; i++) new_pages[i] = pages[i];
            for (uint32_t i = page_count; i < new_page_count; i++) new_pages[i] = nullptr;
            page_count = new_page_count;
            pages = new_pages;
        }
        if (pages[page_index] == nullptr) {
            pages[page_index] = new uint8_t[P]();
            if (pages[page_index] == nullptr) throw MEMORY_EXAUSTED;
        }
        if (i > limit) limit = i;
        return pages[page_index][byte_index];
    }
    inline uint256_t load(uint32_t offset) const {
        uint8_t buffer[32];
        dump(offset, 32, buffer);
        return uint256_t::from(buffer);
    }
    inline void store(uint32_t offset, const uint256_t& v) {
        uint8_t buffer[32];
        word(v, buffer, 32);
        burn(offset, 32, buffer);
    }
    inline void dump(uint32_t offset, uint32_t size, uint8_t *buffer) const {
        for (uint32_t i = 0; i < size; i++) {
            buffer[i] = (*this)[offset+i];
        }
    }
    inline void burn(uint32_t offset, uint32_t size, const uint8_t *buffer) {
        for (uint32_t i = 0; i < size; i++) {
            (*this)[offset+i] = buffer[i];
        }
    }
};

struct account {
    uint256_t nonce;
    uint256_t balance;
    uint8_t *code;
    uint32_t code_size;
    uint256_t code_hash;
};

struct substate {
    uint256_t *destruct;
    void *log_series;
    uint256_t *touched;
    uint256_t refund;
};

class Storage {
protected:
    virtual struct account *find_account(const uint256_t &address) = 0;
public:
    inline uint256_t balance(const uint256_t &v) {
        struct account *account = find_account(v);
        return account == nullptr ? 0 : account->balance;
    }
    inline const uint8_t* code(const uint256_t &v) {
        struct account *account = find_account(v);
        return account == nullptr ? nullptr : account->code;
    }
    inline uint32_t code_size(const uint256_t &v) {
        struct account *account = find_account(v);
        return account == nullptr ? 0 : account->code_size;
    }
    inline uint256_t code_hash(const uint256_t &v) {
        struct account *account = find_account(v);
        return account == nullptr ? 0 : account->code_hash;
    }
    virtual const uint256_t& load(const uint256_t &account, const uint256_t &address) = 0;
    virtual void store(const uint256_t &account, const uint256_t &address, const uint256_t& v) = 0;
};

class Block {
public:
    virtual uint256_t chainid() = 0;
    virtual uint32_t timestamp() = 0;
    virtual uint32_t number() = 0;
    virtual uint256_t coinbase() = 0;
    virtual uint256_t difficulty() = 0;
    virtual uint256_t hash(uint32_t number) = 0;
};

static void vm_run(Block &block, Storage &storage, const uint8_t *code, const uint32_t code_size, const uint8_t *call_data, const uint32_t call_size, uint8_t *return_data, const uint8_t return_size, uint32_t depth)
{
    if (depth == 1024) return;
    uint256_t gas;
    uint256_t gas_limit;
    uint256_t gas_price;
    uint256_t owner_address;
    uint256_t origin_address;
    uint256_t caller_address;
    uint256_t call_value;
    // permissions

    bool is_static = false;

    uint32_t pc = 0;
    Stack stack;
    Memory memory;
    for (;;) {
        uint8_t opc = pc <= code_size ? code[pc] : STOP;
        std::cout << opcodes[opc] << std::endl;
        switch (opc) {
        case STOP: { /* hreturn = [] */ return; }
        case ADD: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 + v2); pc++; break; }
        case MUL: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 * v2); pc++; break; }
        case SUB: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 - v2); pc++; break; }
        case DIV: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 / v2); pc++; break; }
        case SDIV: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            bool is_neg1 = (v1[0] & 0x80) > 0;
            bool is_neg2 = (v2[0] & 0x80) > 0;
            if (is_neg1) v1 = -v1;
            if (is_neg2) v2 = -v2;
            uint256_t v3 = v1 / v2;
            if (is_neg1 != is_neg2) v3 = -v3;
            stack.push(v3);
            pc++;
            break;
        }
        case MOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 % v2); pc++; break; }
        case SMOD: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            bool is_neg1 = (v1[0] & 0x80) > 0;
            bool is_neg2 = (v2[0] & 0x80) > 0;
            if (is_neg1) v1 = -v1;
            if (is_neg2) v2 = -v2;
            uint256_t v3 = v1 % v2;
            if (is_neg1) v3 = -v3;
            stack.push(v3);
            pc++;
            break;
        }
        case ADDMOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(); stack.push(uint256_t::addmod(v1, v2, v3)); pc++; break; }
        case MULMOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(); stack.push(uint256_t::mulmod(v1, v2, v3)); pc++; break; }
        case EXP: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(uint256_t::pow(v1, v2)); pc++; break; }
        case SIGNEXTEND: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v2.sigext(v1.cast32())); pc++; break; }
        case LT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 < v2); pc++; break; }
        case GT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 > v2); pc++; break; }
        case SLT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1.sigflip() < v2.sigflip()); pc++; break; }
        case SGT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1.sigflip() > v2.sigflip()); pc++; break; }
        case EQ: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 == v2); pc++; break; }
        case ISZERO: { uint256_t v1 = stack.pop(); stack.push(v1 == 0); pc++; break; }
        case AND: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 & v2); pc++; break; }
        case OR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 | v2); pc++; break; }
        case XOR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 ^ v2); pc++; break; }
        case NOT: { uint256_t v1 = stack.pop(), v2 = ~v1; stack.push(v2); pc++; break; }
        case BYTE: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v2[v1.cast32()]); pc++; }
        case SHL: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v2 << v1.cast32()); pc++; break; }
        case SHR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v2 >> v1.cast32()); pc++; break; }
        case SAR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(uint256_t::sar(v2, v1.cast32())); pc++; break; }
        case SHA3: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            if (buffer == nullptr) throw MEMORY_EXAUSTED;
            memory.dump(offset, size, buffer);
            uint256_t v3 = sha3(buffer, size);
            stack.push(v3);
            pc++;
            break;
        }
        case ADDRESS: { stack.push(owner_address); pc++; break; }
        case BALANCE: { uint256_t v1 = stack.pop(); stack.push(storage.balance(v1)); pc++; break; }
        case ORIGIN: { stack.push(origin_address); pc++; break; }
        case CALLER: { stack.push(caller_address); pc++; break; }
        case CALLVALUE: { stack.push(call_value); pc++; break; }
        case CALLDATALOAD: { uint256_t v1 = stack.pop(); stack.push(call_data[v1.cast32()]); pc++; break; /* review */}
        case CALLDATASIZE: { stack.push(call_size); pc++; break; }
        case CALLDATACOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset1 = v1.cast32();
            uint32_t offset2 = v2.cast32();
            uint32_t size = v3.cast32();
            if (offset2 + size > call_size) size = call_size - offset2;
            memory.burn(offset1, size, &call_data[offset2]);
            pc++;
            break;
        }
        case CODESIZE: { stack.push(code_size); pc++; break; }
        case CODECOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset1 = v1.cast32();
            uint32_t offset2 = v2.cast32();
            uint32_t size = v3.cast32();
            if (offset2 + size > code_size) size = code_size - offset2;
            memory.burn(offset1, size, &code[offset2]);
            pc++;
            break;
        }
        case GASPRICE: { stack.push(gas_price); pc++; break; }
        case EXTCODESIZE: { uint256_t v1 = stack.pop(); stack.push(storage.code_size(v1)); pc++; break; }
        case EXTCODECOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t address = v1.cast32();
            uint32_t offset1 = v2.cast32();
            uint32_t offset2 = v3.cast32();
            uint32_t size = v4.cast32();
            const uint8_t *code = storage.code(address);
            const uint32_t code_size = storage.code_size(address);
            if (offset2 + size > code_size) size = code_size - offset2;
            memory.burn(offset1, size, &code[offset2]);
            pc++;
            break;
        }
        case RETURNDATASIZE: { stack.push(return_size); pc++; break; }
        case RETURNDATACOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset1 = v1.cast32();
            uint32_t offset2 = v2.cast32();
            uint32_t size = v3.cast32();
            if (offset2 + size > return_size) size = return_size - offset2;
            memory.burn(offset1, size, &return_data[offset2]);
            pc++;
            break;
        }
        case EXTCODEHASH: { uint256_t v1 = stack.pop(); stack.push(storage.code_hash(v1)); pc++; break; }
        case BLOCKHASH: { uint256_t v1 = stack.pop(); stack.push(block.hash(v1.cast32())); pc++; break; }
        case COINBASE: { stack.push(block.coinbase()); pc++; break; }
        case TIMESTAMP: { stack.push(block.timestamp()); pc++; break; }
        case NUMBER: { stack.push(block.number()); pc++; break; }
        case DIFFICULTY: { stack.push(block.difficulty()); pc++; break; }
        case GASLIMIT: { stack.push(gas_limit); pc++; break; }
        case CHAINID: { stack.push(block.chainid()); pc++; break; }
        case SELFBALANCE: { stack.push(storage.balance(owner_address)); pc++; break; }
        case POP: { stack.pop(); pc++; break; }
        case MLOAD: { uint256_t v1 = stack.pop(); stack.push(memory.load(v1.cast32())); pc++; break; }
        case MSTORE: { uint256_t v1 = stack.pop(), v2 = stack.pop(); memory.store(v1.cast32(), v2); pc++; break; }
        case MSTORE8: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint8_t buffer[1];
            buffer[0] = v2.cast32() & 0xff;
            memory.burn(v1.cast32(), 1, buffer);
            pc++;
            break;
        }
        case SLOAD: { uint256_t v1 = stack.pop(); stack.push(storage.load(owner_address, v1)); pc++; break; }
        case SSTORE: {
            if (is_static) throw ILLEGAL_UPDATE;
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            storage.store(owner_address, v1, v2);
            pc++;
            break;
        }
        case JUMP: {
            uint256_t v1 = stack.pop();
            pc = v1.cast32();
            uint8_t opc = pc <= code_size ? code[pc] : STOP;
            if (opc != JUMPDEST) throw ILLEGAL_TARGET;
            break;
        }
        case JUMPI: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            if (v1 != 0) {
                pc = v2.cast32();
                uint8_t opc = pc <= code_size ? code[pc] : STOP;
                if (opc != JUMPDEST) throw ILLEGAL_TARGET;
                break;
            }
            pc++;
            break;
        }
        case PC: { stack.push(pc); pc++; break; }
        case MSIZE: { stack.push(memory.size()); pc++; break; }
        case GAS: { stack.push(gas); pc++; break; }
        case JUMPDEST: { pc++; break; }
        case PUSH1: { const int n = 1; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH2: { const int n = 2; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH3: { const int n = 3; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH4: { const int n = 4; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH5: { const int n = 5; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH6: { const int n = 6; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH7: { const int n = 7; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH8: { const int n = 8; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH9: { const int n = 9; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH10: { const int n = 10; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH11: { const int n = 11; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH12: { const int n = 12; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH13: { const int n = 13; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH14: { const int n = 14; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH15: { const int n = 15; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH16: { const int n = 16; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH17: { const int n = 17; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH18: { const int n = 18; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH19: { const int n = 19; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH20: { const int n = 20; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH21: { const int n = 21; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH22: { const int n = 22; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH23: { const int n = 23; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH24: { const int n = 24; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH25: { const int n = 25; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH26: { const int n = 26; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH27: { const int n = 27; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH28: { const int n = 28; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH29: { const int n = 29; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH30: { const int n = 30; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH31: { const int n = 31; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
        case PUSH32: { const int n = 32; uint256_t v1 = uint256_t::from(&code[pc+1], n); stack.push(v1); pc += 1 + n; break; }
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
        case LOG0: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32();
            uint32_t size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            // log owner_address, buffer
            pc++;
            break;
        }
        case LOG1: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset = v1.cast32();
            uint32_t size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            // log owner_address, v3, buffer
            pc++;
            break;
        }
        case LOG2: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t offset = v1.cast32();
            uint32_t size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            // log owner_address, v3, v4, buffer
            pc++;
            break;
        }
        case LOG3: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop(), v5 = stack.pop();
            uint32_t offset = v1.cast32();
            uint32_t size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            // log owner_address, v3, v4, v5, buffer
            pc++;
            break;
        }
        case LOG4: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop(), v5 = stack.pop(), v6 = stack.pop();
            uint32_t offset = v1.cast32();
            uint32_t size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            // log owner_address, v3, v4, v5, v6, buffer
            pc++;
            break;
        }
        case CREATE: {
            if (is_static) throw ILLEGAL_UPDATE;
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset = v2.cast32();
            uint32_t size = v3.cast32();
            // create_contract(v1, offset, size);
            pc++;
            break;
        }
        case CALL: {
            uint256_t call_gas = stack.pop();
            uint256_t code_address = stack.pop();
            uint256_t value = stack.pop();
            uint256_t in_offset = stack.pop();
            uint256_t in_size = stack.pop();
            uint256_t out_offset = stack.pop();
            uint256_t out_size = stack.pop();
            if (is_static && value != 0) throw ILLEGAL_UPDATE;

            const uint32_t return_size = out_size.cast32();
            uint8_t return_data[return_size];

            const uint32_t call_size = in_size.cast32();
            uint8_t call_data[call_size];
            memory.dump(in_offset.cast32(), call_size, call_data);

            const uint8_t *code = storage.code(code_address);
            const uint32_t code_size = storage.code_size(code_address);
            vm_run(block, storage, code, code_size, call_data, call_size, return_data, return_size, depth+1);

            memory.burn(out_offset.cast32(), return_size, return_data);

            pc++;
            break;
        }
        case CALLCODE: throw UNIMPLEMENTED;
        case RETURN: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            pc++;
            return;
        }
        case DELEGATECALL: throw UNIMPLEMENTED;
        case CREATE2: {
            if (is_static) throw ILLEGAL_UPDATE;
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t offset = v2.cast32();
            uint32_t size = v3.cast32();
            // create_contract(v1, offset, size, v4);
            pc++;
            break;
        }
        case STATICCALL: throw UNIMPLEMENTED;
        case REVERT: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            pc++;
            return;
        }
        case SELFDESTRUCT: { return; }
        default: throw INVALID_OPCODE;
        }
    }
}

class _Storage : public Storage {
private:
    struct account *find_account(const uint256_t &address) {
        throw UNIMPLEMENTED;
    }
public:
    const uint256_t& load(const uint256_t &account, const uint256_t &address) {
        throw UNIMPLEMENTED;
    }
    void store(const uint256_t &account, const uint256_t &address, const uint256_t& v) {
        throw UNIMPLEMENTED;
    }
};

class _Block : public Block {
public:
    uint256_t chainid() {
        return 0; // configurable
    }
    uint32_t timestamp() {
        return 0; // eos block
    }
    uint32_t number() {
        return 0; // eos block
    }
    uint256_t coinbase() {
        return 0; // static
    }
    uint256_t difficulty() {
        return 0; // static
    }
    uint256_t hash(uint32_t number) {
        return 0; // static
    }
};

struct message {
    uint8_t opcode;
    uint256_t gas;
    uint256_t code_address;
    uint256_t endowment;
    uint32_t in_offset;
    uint32_t in_size;
    uint32_t out_offset;
    uint32_t out_size;
};

static void raw(const uint8_t *buffer, uint32_t size)
{
    _Block block;
    _Storage storage;
    struct txn txn = decode_txn(buffer, size);
    // call
    if (txn.has_to) {
        const uint32_t code_size = storage.code_size(txn.to);
        const uint8_t *code = storage.code(txn.to);
        if (code != nullptr) {
            const uint32_t call_size = 0;
            const uint8_t *call_data = nullptr;
            const uint32_t return_size = 0;
            uint8_t *return_data = nullptr;
            vm_run(block, storage, code, code_size, call_data, call_size, return_data, return_size, 0);
        } else {
            // token transfer
        }
    } else {
        // contract creation
    }
}

/* main */

static inline int hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return -1;
}

static inline int parse_hex(const char *hexstr, uint8_t *buffer, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        int hi = hex(hexstr[2*i]);
        int lo = hex(hexstr[2*i+1]);
        if (hi < 0 || lo < 0) return 0;
        buffer[i] = hi << 4 | lo;
    }
    return 1;
}

int main(int argc, const char *argv[])
{
    const char *progname = argv[0];
    if (argc < 2) { std::cerr << "usage: " << progname << " <hex>" << std::endl; return 1; }
    const char *hexstr = argv[1];
    int len = std::strlen(hexstr);
    uint32_t size = len / 2;
    uint8_t buffer[size];
    if (len % 2 > 0 || !parse_hex(hexstr, buffer, size)) { std::cerr << progname << ": invalid input" << std::endl; return 1; }
    try {
        raw(buffer, size);
    } catch (Error e) {
        std::cerr << progname << ": error " << errors[e] << std::endl; return 1;
    }
    return 0;
}
