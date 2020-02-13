#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdint.h>

/* error */

enum Error {
    CODE_CONFLICT = 1, // VM
    DIVIDES_ZERO,
    GAS_EXAUSTED, // VM
    MEMORY_EXAUSTED,
    ILLEGAL_TARGET, // VM
    ILLEGAL_UPDATE, // VM
    INSUFFICIENT_BALANCE,
    INSUFFICIENT_SPACE,
    INVALID_ENCODING,
    INVALID_OPCODE, // VM
    INVALID_SIGNATURE,
    INVALID_SIZE,
    INVALID_TRANSACTION,
    NONCE_MISMATCH,
    OUTOFBOUND_INDEX, // VM
    RECURSION_LIMITED, // VM
    STACK_OVERFLOW, // VM
    STACK_UNDERFLOW, // VM
    UNKNOWN_FILE,
    UNIMPLEMENTED,
};

static const char *errors[UNIMPLEMENTED+1] = {
    nullptr,
    "CODE_CONFLICT",
    "DIVIDES_ZERO",
    "GAS_EXAUSTED",
    "MEMORY_EXAUSTED",
    "ILLEGAL_TARGET",
    "ILLEGAL_UPDATE",
    "INSUFFICIENT_BALANCE",
    "INSUFFICIENT_SPACE",
    "INVALID_ENCODING",
    "INVALID_OPCODE",
    "INVALID_SIGNATURE",
    "INVALID_SIZE",
    "INVALID_TRANSACTION",
    "NONCE_MISMATCH",
    "OUTOFBOUND_INDEX",
    "RECURSION_LIMITED",
    "STACK_OVERFLOW",
    "STACK_UNDERFLOW",
    "UNKNOWN_FILE",
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
        for (int i = W-1; i >= 0; i--) {
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
    inline uint32_t cast32() const { return data[0]; }
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
            uint64_t n = data[i] + (v.data[i] + carry);
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
            if (base == 0) continue;
            uint64_t carry = 0;
            for (int i = j; i < W; i++) {
                uint64_t n = data[i] + base * t.data[i-j] + carry;
                data[i] = n & 0xffffffff;
                carry = n >> 32;
            }
        }
        return *this;
    }
    inline uintX_t& operator/=(const uintX_t& v) { uintX_t t1 = *this, t2; divmod(t1, v, *this, t2); return *this; }
    inline uintX_t& operator%=(const uintX_t& v) { uintX_t t1 = *this, t2; divmod(t1, v, t2, *this); return *this; }
    inline uintX_t& operator&=(const uintX_t& v) { for (int i = 0; i < W; i++) data[i] &= v.data[i]; return *this; }
    inline uintX_t& operator|=(const uintX_t& v) { for (int i = 0; i < W; i++) data[i] |= v.data[i]; return *this; }
    inline uintX_t& operator^=(const uintX_t& v) { for (int i = 0; i < W; i++) data[i] ^= v.data[i]; return *this; }
    inline uintX_t& operator<<=(int n) {
        if (n < 0 || n >= X) throw OUTOFBOUND_INDEX;
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
        if (n < 0 || n >= X) throw OUTOFBOUND_INDEX;
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
        if (n < 0 || n >= X) throw OUTOFBOUND_INDEX;
        if (n == 0) return v;
        uintX_t t = v;
        bool is_neg = (t[0] & 0x80) > 0;
        if (is_neg) t = ~t;
        t >>= n;
        if (is_neg) t = ~t;
        return t;
    }
    static inline void divmod(const uintX_t &num, const uintX_t &div, uintX_t &quo, uintX_t &rem) {
        if (div == 0) throw DIVIDES_ZERO;
        quo = 0;
        rem = num;
        int shift = 0;
        uintX_t<X+32> _num = num;
        uintX_t<X+32> _div = div;
        while (_div <= _num) {
            _div <<= 32;
            shift += 32;
        }
        if (shift == 0) return;
        while (shift >= 0) {
            if (_num >= _div) {
                _num -= _div;
                int i = shift / 32;
                int j = shift % 32;
                quo.data[i] |= 1 << j;
            }
            _div >>= 1;
            shift--;
        }
        rem = _num;
    }
    static inline const uintX_t pow(const uintX_t &v1, const uintX_t &v2) {
        uintX_t x1 = 1;
        uintX_t x2 = v1;
        for (int n = X - 1; n >= 0; n--) {
            int i = n / 32;
            int j = n % 32;
            uintX_t t = (v2.data[i] & (1 << j)) == 0 ? x1 : x2;
            x1 *= t;
            x2 *= t;
        }
        return x1;
    }
    static inline const uintX_t addmod(const uintX_t &v1, const uintX_t &v2, const uintX_t &v3) {
        uintX_t<X+32> _v1 = v1;
        uintX_t<X+32> _v2 = v2;
        uintX_t<X+32> _v3 = v3;
        return (_v1 + _v2) % _v3;
    }
    static inline const uintX_t mulmod(const uintX_t &v1, const uintX_t &v2, const uintX_t &v3) {
        uintX_t<2*X> _v1 = v1;
        uintX_t<2*X> _v2 = v2;
        uintX_t<2*X> _v3 = v3;
        return (_v1 * _v2) % _v3;
    }
    static inline const uintX_t from(const char *buffer) { return from(buffer, B); }
    static inline const uintX_t from(const char *buffer, int size) { return from((const uint8_t*)buffer, size); }
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
    static inline void to(const uintX_t &v, uint8_t *buffer) { to(v, buffer, B); }
    static inline void to(const uintX_t &v, uint8_t *buffer, int size) {
        if (size < 0 || size > B) throw INVALID_SIZE;
        for (int j = 0; j < size; j++) {
            int i = j + B - size;
            buffer[j] = v[i];
        }
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

/* crypto */

static inline uint64_t rot(uint64_t x, int y) { return y > 0 ? (x >> (64 - y)) ^ (x << y) : x; }

static inline uint64_t b2w64le(const uint8_t *b)
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

static inline void w2b64le(uint64_t w, uint8_t *b)
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
        uint64_t bitsize = 8 * size;
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
    static const uint64_t RC[24] = {
        0x0000000000000001L, 0x0000000000008082L, 0x800000000000808aL,
        0x8000000080008000L, 0x000000000000808bL, 0x0000000080000001L,
        0x8000000080008081L, 0x8000000000008009L, 0x000000000000008aL,
        0x0000000000000088L, 0x0000000080008009L, 0x000000008000000aL,
        0x000000008000808bL, 0x800000000000008bL, 0x8000000000008089L,
        0x8000000000008003L, 0x8000000000008002L, 0x8000000000000080L,
        0x000000000000800aL, 0x800000008000000aL, 0x8000000080008081L,
        0x8000000000008080L, 0x0000000080000001L, 0x8000000080008008L,
    };
    static const uint8_t R[5][5] = {
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
            w[i] = b2w64le(&message[8*(j+i)]);
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
    w2b64le(s[0][0], &output[0]);
    w2b64le(s[1][0], &output[8]);
    w2b64le(s[2][0], &output[16]);
    w2b64le(s[3][0], &output[24]);
    w2b64le(s[4][0], &output[32]);
    w2b64le(s[0][1], &output[40]);
    w2b64le(s[1][1], &output[48]);
    w2b64le(s[2][1], &output[56]);
}

static uint256_t sha3(const uint8_t *buffer, uint32_t size)
{
    uint8_t output[64];
    sha3(buffer, size, false, 1088, 0x01, output);
    return uint256_t::from(output);
}

static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & (y ^ z)) ^ z; }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ ((x ^ y) & z); }
static inline uint32_t rtr(uint32_t x, int y) { return (x >> y) ^ (x << (32 - y)); }
static inline uint32_t ep0(uint32_t x) { return rtr(x, 2) ^ rtr(x, 13) ^ rtr(x, 22); }
static inline uint32_t ep1(uint32_t x) { return rtr(x, 6) ^ rtr(x, 11) ^ rtr(x, 25); }
static inline uint32_t sig0(uint32_t x) { return rtr(x, 7) ^ rtr(x, 18) ^ (x >> 3); }
static inline uint32_t sig1(uint32_t x) { return rtr(x, 17) ^ rtr(x, 19) ^ (x >> 10); }

static inline uint32_t b2w32be(const uint8_t *b)
{
    return 0
        | ((uint64_t)b[0] << 24)
        | ((uint64_t)b[1] << 16)
        | ((uint64_t)b[2] << 8)
        | (uint64_t)b[3];
}

static inline void w2b32be(uint32_t w, uint8_t *b)
{
    b[3] = (uint8_t)w;
    b[2] = (uint8_t)(w >> 8);
    b[1] = (uint8_t)(w >> 16);
    b[0] = (uint8_t)(w >> 24);
}

static void sha256(const uint8_t *message, uint32_t size, bool compressed, uint8_t *output)
{
    if (!compressed) {
        uint64_t bitsize = 8 * size;
        uint32_t modulo = (size + 1 + 8) % 64;
        uint32_t padding = modulo > 0 ? 64 - modulo : 0;
        uint32_t b_len = size + 1 + padding + 8;
        uint8_t b[b_len];
        for (uint32_t i = 0; i < size; i++) b[i] = message[i];
        for (uint32_t i = size; i < b_len; i++) b[i] = 0;
        b[size] = 0x80;
        w2b32be(bitsize >> 32, &b[b_len-8]);
        w2b32be(bitsize, &b[b_len-4]);
        sha256(b, b_len, true, output);
        return;
    }
    static const uint32_t S[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };
    uint32_t s0 = S[0];
    uint32_t s1 = S[1];
    uint32_t s2 = S[2];
    uint32_t s3 = S[3];
    uint32_t s4 = S[4];
    uint32_t s5 = S[5];
    uint32_t s6 = S[6];
    uint32_t s7 = S[7];
    for (uint32_t j = 0; j < size/4; j += 16) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = b2w32be(&message[4*(j+i)]);
        }
        for (int i = 16; i < 64; i++) {
            w[i] = w[i-16] + sig0(w[i-15]) + w[i-7] + sig1(w[i-2]);
        }
        uint32_t a = s0;
        uint32_t b = s1;
        uint32_t c = s2;
        uint32_t d = s3;
        uint32_t e = s4;
        uint32_t f = s5;
        uint32_t g = s6;
        uint32_t h = s7;
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + ep1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = ep0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        s0 += a;
        s1 += b;
        s2 += c;
        s3 += d;
        s4 += e;
        s5 += f;
        s6 += g;
        s7 += h;
    }
    w2b32be(s0, &output[0]);
    w2b32be(s1, &output[4]);
    w2b32be(s2, &output[8]);
    w2b32be(s3, &output[12]);
    w2b32be(s4, &output[16]);
    w2b32be(s5, &output[20]);
    w2b32be(s6, &output[24]);
    w2b32be(s7, &output[28]);
}

static uint256_t sha256(const uint8_t *buffer, uint32_t size)
{
    uint8_t output[32];
    sha256(buffer, size, false, output);
    return uint256_t::from(output);
}

static inline uint32_t b2w32le(const uint8_t *b)
{
    return 0
        | ((uint64_t)b[3] << 24)
        | ((uint64_t)b[2] << 16)
        | ((uint64_t)b[1] << 8)
        | (uint64_t)b[0];
}

static inline void w2b32le(uint32_t w, uint8_t *b)
{
    b[0] = (uint8_t)w;
    b[1] = (uint8_t)(w >> 8);
    b[2] = (uint8_t)(w >> 16);
    b[3] = (uint8_t)(w >> 24);
}

static inline uint32_t rol(uint32_t x, int n) { return (x << n) ^ (x >> (32 - n)); }

static inline uint32_t f(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static inline uint32_t g(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t h(uint32_t x, uint32_t y, uint32_t z) { return (x | ~y) ^ z; }
static inline uint32_t i(uint32_t x, uint32_t y, uint32_t z) { return (x & z) ^ (y & ~z); }
static inline uint32_t j(uint32_t x, uint32_t y, uint32_t z) { return x ^ (y | ~z); }

static void ripemd160(const uint8_t *message, uint32_t size, bool compressed, uint8_t *output)
{
    if (!compressed) {
        uint64_t bitsize = 8 * size;
        uint32_t modulo = (size + 1 + 8) % 64;
        uint32_t padding = modulo > 0 ? 64 - modulo : 0;
        uint32_t b_len = size + 1 + padding + 8;
        uint8_t b[b_len];
        for (uint32_t i = 0; i < size; i++) b[i] = message[i];
        for (uint32_t i = size; i < b_len; i++) b[i] = 0;
        b[size] = 0x80;
        w2b32le(bitsize, &b[b_len-8]);
        w2b32le(bitsize >> 32, &b[b_len-4]);
        ripemd160(b, b_len, true, output);
        return;
    }
    static uint32_t (*F1[5])(uint32_t,uint32_t,uint32_t) = { f, g, h, i, j };
    static const uint32_t K1[5] = { 0x00000000, 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xa953fd4e };
    static const uint8_t O1[5][16] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
        { 7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5, 2, 14, 11, 8 },
        { 3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12 },
        { 1, 9, 11, 10, 0, 8, 12, 4, 13, 3, 7, 15, 14, 5, 6, 2 },
        { 4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13 },
    };
    static const uint8_t P1[5][16] = {
        { 11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8 },
        { 7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15, 9, 11, 7, 13, 12 },
        { 11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5 },
        { 11, 12, 14, 15, 14, 15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12 },
        { 9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6 },
    };
    static uint32_t (*F2[5])(uint32_t,uint32_t,uint32_t) = { j, i, h, g, f };
    const uint32_t K2[5] = { 0x50a28be6, 0x5c4dd124, 0x6d703ef3, 0x7a6d76e9, 0x00000000 };
    const uint8_t O2[5][16] = {
        { 5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12 },
        { 6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12, 4, 9, 1, 2 },
        { 15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13 },
        { 8, 6, 4, 1, 3, 11, 15, 0, 5, 12, 2, 13, 9, 7, 10, 14 },
        { 12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11 },
    };
    const uint8_t P2[5][16] = {
        { 8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6 },
        { 9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12, 7, 6, 15, 13, 11 },
        { 9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5 },
        { 15, 5, 8, 11, 14, 14, 6, 14, 6, 9, 12, 9, 12, 5, 15, 8 },
        { 8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11 },
    };
    const uint32_t S[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
    uint32_t s0 = S[0], s1 = S[1], s2 = S[2], s3 = S[3], s4 = S[4];
    for (uint32_t j = 0; j < size/4; j += 16) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = b2w32le(&message[4*(j+i)]);
        }
        for (int i = 16; i < 64; i++) {
            w[i] = 0;
        }
        uint32_t u0, u1, u2, u3, u4;
        {
            uint32_t a = s0, b = s1, c = s2, d = s3, e = s4;
            for (int i = 0; i < 5; i++) {
                uint32_t (*f)(uint32_t,uint32_t,uint32_t) = F1[i];
                uint32_t k = K1[i];
                const uint8_t *o = O1[i];
                const uint8_t *p = P1[i];
                for (int j = 0; j < 16; j++) {
                    a = rol(a + f(b, c, d) + w[o[j]] + k, p[j]) + e;
                    c = rol(c, 10);
                    uint32_t t = e; e = d; d = c; c = b; b = a; a = t;
                }
            }
            u0 = a; u1 = b; u2 = c; u3 = d; u4 = e;
        }
        uint32_t v0, v1, v2, v3, v4;
        {
            uint32_t a = s0, b = s1, c = s2, d = s3, e = s4;
            for (int i = 0; i < 5; i++) {
                uint32_t (*f)(uint32_t,uint32_t,uint32_t) = F2[i];
                uint32_t k = K2[i];
                const uint8_t *o = O2[i];
                const uint8_t *p = P2[i];
                for (int j = 0; j < 16; j++) {
                    a = rol(a + f(b, c, d) + w[o[j]] + k, p[j]) + e;
                    c = rol(c, 10);
                    uint32_t t = e; e = d; d = c; c = b; b = a; a = t;
                }
            }
            v0 = a; v1 = b; v2 = c; v3 = d; v4 = e;
        }
        s0 += u1 + v2;
        s1 += u2 + v3;
        s2 += u3 + v4;
        s3 += u4 + v0;
        s4 += u0 + v1;
        uint32_t t = s0; s0 = s1; s1 = s2; s2 = s3; s3 = s4; s4 = t;
    }
    w2b32le(s0, &output[0]);
    w2b32le(s1, &output[4]);
    w2b32le(s2, &output[8]);
    w2b32le(s3, &output[12]);
    w2b32le(s4, &output[16]);
}

static uint160_t ripemd160(const uint8_t *buffer, uint32_t size)
{
    uint8_t output[20];
    ripemd160(buffer, size, false, output);
    return uint160_t::from(output);
}

static inline uint64_t ror(uint64_t x, int n) { return (x << (64 - n)) ^ (x >> n); }

static inline void mix(
    uint64_t a, uint64_t b, uint64_t c, uint64_t d,
    uint64_t x, uint64_t y,
    uint64_t &_a, uint64_t &_b, uint64_t &_c, uint64_t &_d) {
    a = a + b + x;
    d = ror(d ^ a, 32);
    c = c + d;
    b = ror(b ^ c, 24);
    a = a + b + y;
    d = ror(d ^ a, 16);
    c = c + d;
    b = ror(b ^ c, 63);
    _a = a; _b = b; _c = c; _d = d;
}

static void blake2f(const uint32_t ROUNDS,
    uint64_t &h0, uint64_t &h1, uint64_t &h2, uint64_t &h3,
    uint64_t &h4, uint64_t &h5, uint64_t &h6, uint64_t &h7,
    uint64_t w[16], uint64_t t0, uint64_t t1, bool last_chunk) {
    static const uint8_t SIGMA[10][16] = {
        {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
        { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
        { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
        {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
        {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
        {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
        { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
        { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
        {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
        { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
    };
    static const uint64_t iv[8] = {
        0x6a09e667f3bcc908, 0xbb67ae8584caa73b,
        0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
        0x510e527fade682d1, 0x9b05688c2b3e6c1f,
        0x1f83d9abfb41bd6b, 0x5be0cd19137e2179
    };
    uint64_t v0 = h0,  v1 = h1, v2 = h2, v3 = h3, v4 = h4, v5 = h5, v6 = h6, v7 = h7;
    uint64_t v8 = iv[0], v9 = iv[1], v10 = iv[2], v11 = iv[3], v12 = iv[4], v13 = iv[5], v14 = iv[6], v15 = iv[7];
    v12 ^= t0;
    v13 ^= t1;
    if (last_chunk) v14 ^= 0xffffffffffffffffL;
    for (uint32_t r = 0; r < ROUNDS; r++) {
        const uint8_t *indexes = SIGMA[r % 10];
        uint64_t m[16];
        for (int i = 0; i < 16; i++) m[i] = w[indexes[i]];
        uint64_t _v0, _v1, _v2, _v3, _v4, _v5, _v6, _v7;
        uint64_t _v8, _v9, _v10, _v11, _v12, _v13, _v14, _v15;
        mix(v0, v4, v8, v12, m[0], m[1], _v0, _v4, _v8, _v12);
        mix(v1, v5, v9, v13, m[2], m[3], _v1, _v5, _v9, _v13);
        mix(v2, v6, v10, v14, m[4], m[5], _v2, _v6, _v10, _v14);
        mix(v3, v7, v11, v15, m[6], m[7], _v3, _v7, _v11, _v15);
        mix(_v0, _v5, _v10, _v15, m[8], m[9], _v0, _v5, _v10, _v15);
        mix(_v1, _v6, _v11, _v12, m[10], m[11], _v1, _v6, _v11, _v12);
        mix(_v2, _v7, _v8, _v13, m[12], m[13], _v2, _v7, _v8, _v13);
        mix(_v3, _v4, _v9, _v14, m[14], m[15], _v3, _v4, _v9, _v14);
        v0 = _v0; v1 = _v1; v2 = _v2; v3 = _v3; v4 = _v4; v5 = _v5; v6 = _v6; v7 = _v7;
        v8 = _v8; v9 = _v9; v10 = _v10; v11 = _v11; v12 = _v12; v13 = _v13; v14 = _v14; v15 = _v15;
    }
    h0 ^= v0 ^ v8;
    h1 ^= v1 ^ v9;
    h2 ^= v2 ^ v10;
    h3 ^= v3 ^ v11;
    h4 ^= v4 ^ v12;
    h5 ^= v5 ^ v13;
    h6 ^= v6 ^ v14;
    h7 ^= v7 ^ v15;
}

/* secp256k1 */

const uint256_t _N[8] = {
    // secp
    uint256_t::from("\xfe\xff\xff\xfc\x2f", 5).sigext(27),
    uint256_t::from("\xfe\xba\xae\xdc\xe6\xaf\x48\xa0\x3b\xbf\xd2\x5e\x8c\xd0\x36\x41\x41", 17).sigext(15),
    uint256_t::from("\x79\xbe\x66\x7e\xf9\xdc\xbb\xac\x55\xa0\x62\x95\xce\x87\x0b\x07\x02\x9b\xfc\xdb\x2d\xce\x28\xd9\x59\xf2\x81\x5b\x16\xf8\x17\x98"),
    uint256_t::from("\x48\x3a\xda\x77\x26\xa3\xc4\x65\x5d\xa4\xfb\xfc\x0e\x11\x08\xa8\xfd\x17\xb4\x48\xa6\x85\x54\x19\x9c\x47\xd0\x8f\xfb\x10\xd4\xb8"),
    // bn
    uint256_t::from("\x30\x64\x4e\x72\xe1\x31\xa0\x29\xb8\x50\x45\xb6\x81\x81\x58\x5d\x97\x81\x6a\x91\x68\x71\xca\x8d\x3c\x20\x8c\x16\xd8\x7c\xfd\x47"),
    uint256_t::from("\x30\x64\x4e\x72\xe1\x31\xa0\x29\xb8\x50\x45\xb6\x81\x81\x58\x5d\x28\x33\xe8\x48\x79\xb9\x70\x91\x43\xe1\xf5\x93\xf0\x00\x00\x01"),
    0,
    0,
};

template<int N>
class modN_t {
private:
    static inline const uint256_t &n() { return _N[N]; }
    uint256_t u;
public:
    inline const uint256_t &as256() const { return u; }
    inline modN_t() {}
    inline modN_t(uint32_t v) : u(v) {}
    inline modN_t(const uint256_t &v) : u(v % n()) {}
    inline modN_t(const modN_t &v) : u(v.u) {}
    inline modN_t& operator=(const modN_t& v) { u = v.u; return *this; }
    inline const modN_t operator-() const { return modN_t(n() - u); }
    inline modN_t& operator++() { *this += modN_t(1); return *this; }
    inline modN_t& operator--() { *this -= modN_t(1); return *this; }
    inline const modN_t operator++(int) { const modN_t v = *this; ++(*this); return v; }
    inline const modN_t operator--(int) { const modN_t v = *this; --(*this); return v; }
    inline modN_t& operator+=(const modN_t& v) { *this = *this + v; return *this; }
    inline modN_t& operator-=(const modN_t& v) { *this = *this - v; return *this; }
    inline modN_t& operator*=(const modN_t& v) { *this = *this * v; return *this; }
    inline modN_t& operator/=(const modN_t& v) { *this = *this / v; return *this; }
    friend inline const modN_t operator+(const modN_t& v1, const modN_t& v2) { return modN_t(uint256_t::addmod(v1.u, v2.u, n())); }
    friend inline const modN_t operator-(const modN_t& v1, const modN_t& v2) { return v1 + (-v2); }
    friend inline const modN_t operator*(const modN_t& v1, const modN_t& v2) { return modN_t(uint256_t::mulmod(v1.u, v2.u, n())); }
    friend inline const modN_t operator/(const modN_t& v1, const modN_t& v2) { return v1.u * pow(v2.u, -(modN_t)2); }
    friend inline bool operator==(const modN_t& v1, const modN_t& v2) { return v1.u == v2.u; }
    friend inline bool operator!=(const modN_t& v1, const modN_t& v2) { return v1.u != v2.u; }
    static inline const modN_t pow(const modN_t &v1, const modN_t &v2) {
        modN_t x1 = 1;
        modN_t x2 = v1;
        for (int n = 256 - 1; n >= 0; n--) {
            int i = 31 - n / 8;
            int j = n % 8;
            modN_t t = (v2.u[i] & (1 << j)) == 0 ? x1 : x2;
            x1 *= t;
            x2 *= t;
        }
        return x1;
    }
    friend std::ostream& operator<<(std::ostream &os, const modN_t &v) {
        os << v.u;
        return os;
    }
};

template<int N, int GX, int GY, int A, int B>
class pointN_t {
private:
    static inline const modN_t<N> a() { return A; }
    static inline const modN_t<N> b() { return B; }
    static inline const pointN_t g() { return pointN_t(_N[GX], _N[GY]); }
    bool is_inf;
    modN_t<N> x;
    modN_t<N> y;
public:
    static inline const pointN_t inf() { pointN_t p(0, 0); p.is_inf = true; return p; }
    inline bool belongs() {
	    if (is_inf) return false;
	    return y * y - (x * x * x + a() * x + b()) == 0;
    }
    inline uint512_t as512() const {
        uint8_t buffer[64];
        uint256_t::to(x.as256(), buffer);
        uint256_t::to(y.as256(), &buffer[32]);
        return uint512_t::from(buffer);
    }
    inline pointN_t() {}
    inline pointN_t(const modN_t<N> &_x, const modN_t<N> &_y): is_inf(false), x(_x), y(_y) {}
    inline pointN_t(const pointN_t &p) : is_inf(p.is_inf), x(p.x), y(p.y) {}
    inline pointN_t& operator=(const pointN_t& p) { is_inf = p.is_inf; x = p.x; y = p.y; return *this; }
    inline pointN_t& operator+=(const pointN_t& p) { *this = *this + p; return *this; }
    friend inline const pointN_t operator+(const pointN_t& p1, const pointN_t& p2) {
        if (p1.is_inf) return p2;
        if (p2.is_inf) return p1;
        modN_t<N> x1 = p1.x, y1 = p1.y;
        modN_t<N> x2 = p2.x, y2 = p2.y;
        modN_t<N> l;
        if (x1 == x2) {
            if (y1 != y2) return inf();
            if (y1 == 0) return inf();
            l = (3 * (x1 * x1) + a()) / (2 * y1);
        } else {
            l = (y2 - y1) / (x2 - x1);
        }
        modN_t<N> x3 = l * l - x1 - x2;
        modN_t<N> y3 = l * (x1 - x3) - y1;
        return pointN_t(x3, y3);
    }
    friend inline const pointN_t operator*(const pointN_t& _p, const uint256_t& e) {
        pointN_t p = _p;
        pointN_t q = inf();
        for (int n = 0; n < 256; n++) {
            int i = 31 - n / 8;
            int j = n % 8;
            if ((e[i] & (1 << j)) > 0) q += p;
            p += p;
        }
        return q;
    }
    friend inline bool operator==(const pointN_t& p1, const pointN_t& p2) { return p1.is_inf == p2.is_inf && p1.x == p2.x && p1.y == p2.y; }
    friend inline bool operator!=(const pointN_t& p1, const pointN_t& p2) { return !(p1 == p2); }
    static pointN_t gen(const uint256_t &u) { return  g() * u; }
    static pointN_t find(const modN_t<N> &x, bool is_odd) {
        modN_t<N> poly = x * x * x + a() * x + b();
        modN_t<N> y = modN_t<N>::pow(poly, (modN_t<N>)1 / 4);
        if (is_odd == (y.as256()[31] % 2 == 0)) y = -y;
        return pointN_t(x, y);
    }
    friend std::ostream& operator<<(std::ostream &os, const pointN_t &v) {
        os << v.x << " " << v.y;
        return os;
    }
};

class mod_t : public modN_t<0> {
public:
    inline mod_t() {}
    inline mod_t(uint32_t v) : modN_t(v) {}
    inline mod_t(const uint256_t &v) : modN_t(v) {}
    inline mod_t(const modN_t &v) : modN_t(v) {}
};

class mud_t : public modN_t<1> {
public:
    inline mud_t() {}
    inline mud_t(uint32_t v) : modN_t(v) {}
    inline mud_t(const uint256_t &v) : modN_t(v) {}
    inline mud_t(const modN_t &v) : modN_t(v) {}
};

class point_t : public pointN_t<0, 2, 3, 0, 7> {
public:
    inline point_t() {}
    inline point_t(const mod_t &_x, const mod_t &_y): pointN_t(_x, _y) {}
    inline point_t(const pointN_t &p) : pointN_t(p) {}
};

class mod_bn_t : public modN_t<4> {
public:
    inline mod_bn_t() {}
    inline mod_bn_t(uint32_t v) : modN_t(v) {}
    inline mod_bn_t(const uint256_t &v) : modN_t(v) {}
    inline mod_bn_t(const modN_t &v) : modN_t(v) {}
};

class mud_bn_t : public modN_t<5> {
public:
    inline mud_bn_t() {}
    inline mud_bn_t(uint32_t v) : modN_t(v) {}
    inline mud_bn_t(const uint256_t &v) : modN_t(v) {}
    inline mud_bn_t(const modN_t &v) : modN_t(v) {}
};

class point_bn_t : public pointN_t<4, 6, 7, 0, 3> {
public:
    inline point_bn_t() {}
    inline point_bn_t(const mod_bn_t &_x, const mod_bn_t &_y): pointN_t(_x, _y) {}
    inline point_bn_t(const pointN_t &p) : pointN_t(p) {}
};

static uint256_t ecrecover(const uint256_t &h, const uint256_t &v, const uint256_t &r, const uint256_t &s)
{
    if (v < 27 || v > 28) throw INVALID_SIGNATURE;
    if (r == 0 || mod_t(r).as256() != r) throw INVALID_SIGNATURE;
    if (s == 0 || 2*s < s || mod_t(2*s - 2).as256() != 2*s - 2) throw INVALID_SIGNATURE;
    point_t q = point_t::find(r, v == 28);
    mud_t z = 1 / (mud_t)r;
    mud_t u = -(mud_t)h;
    point_t p = point_t::gen(u.as256()) + q * s;
    point_t t = p * z.as256();
    uint8_t buffer[64];
    uint512_t::to(t.as512(), buffer);
    uint160_t a = (uint160_t)sha3(buffer, 64);
    return (uint256_t)a;
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

static uint32_t dump_nlzint(uint256_t v, uint8_t *b, uint32_t s)
{
    uint32_t l = 32;
    while (l > 0 && v[32 - l] == 0) l--;
    if (b != nullptr) {
        if (s < l) throw INSUFFICIENT_SPACE;
        uint256_t::to(v, &b[s - l], l);
    }
    return l;
}

static uint32_t dump_nlzint(uint256_t v)
{
    return dump_nlzint(v, nullptr, 0);
}

static uint256_t parse_nlzint(const uint8_t *&b, uint32_t &s, uint32_t l)
{
    if (l == 0) return 0;
    if (s < l) throw INVALID_ENCODING;
    if (b[0] == 0) throw INVALID_ENCODING;
    if (l > 32) throw INVALID_ENCODING;
    uint256_t v = uint256_t::from(b, l);
    b += l; s -= l;
    return v;
}

static uint256_t parse_nlzint(const uint8_t *b, uint32_t s)
{
    return parse_nlzint(b, s, s);
}

static uint32_t dump_varlen(uint8_t base, uint8_t c, uint32_t n, uint8_t *b, uint32_t s)
{
    if (base == 0x80 && c < 0x80 && n == 1) return 0;
    uint32_t size = 0;
    if (n > 55) {
        size += dump_nlzint(n, b, s - size);
        n = 55 + size;
    }
    if (b != nullptr) {
        if (s - size < 1) throw INSUFFICIENT_SPACE;
        b[s - size - 1] = base + n;
    }
    size += 1;
    return size;
}

static uint32_t parse_varlen(const uint8_t *&b, uint32_t &s, bool &is_list)
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

static void free_rlp(struct rlp &rlp)
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

static uint32_t dump_rlp(const struct rlp &rlp, uint8_t *b, uint32_t s)
{
    uint32_t size = 0;
    if (rlp.is_list) {
        uint8_t c = 0;
        for (uint32_t i = 0; i < rlp.size; i++) {
            uint32_t j = rlp.size - (i + 1);
            size += dump_rlp(rlp.list[j], b, s - size);
        }
        size += dump_varlen(0xc0, c, size, b, s - size);
    } else {
        uint8_t c = rlp.size > 0 ? rlp.data[0] : 0;
        if (b != nullptr) {
            if (s < rlp.size) throw INSUFFICIENT_SPACE;
            for (uint32_t i = 0; i < rlp.size; i++) {
                uint32_t j = (s - rlp.size) + i;
                b[j] = rlp.data[i];
            }
        }
        size += rlp.size;
        size += dump_varlen(0x80, c, size, b, s - size);
    }
    return size;
}

static void parse_rlp(const uint8_t *&b, uint32_t &s, struct rlp &rlp)
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

static uint32_t encode_txn(const struct txn &txn, uint8_t *buffer, uint32_t size)
{
    struct rlp rlp;
    rlp.is_list = true;
    rlp.size = txn.is_signed ? 9 : 6;
    rlp.list = new struct rlp[rlp.size];
    if (rlp.list == nullptr) throw MEMORY_EXAUSTED;
    for (uint32_t i = 0; i < rlp.size; i++) {
        rlp.list[i].is_list = false;
        rlp.list[i].size = 0;
        rlp.list[i].data = nullptr;
    }
    try {
        rlp.list[0].size = dump_nlzint(txn.nonce);
        rlp.list[1].size = dump_nlzint(txn.gasprice);
        rlp.list[2].size = dump_nlzint(txn.gaslimit);
        rlp.list[3].size = txn.has_to ? 20 : 0;
        rlp.list[4].size = dump_nlzint(txn.value);
        rlp.list[5].size = txn.data_size;
        if (txn.is_signed) {
            rlp.list[6].size = dump_nlzint(txn.v);
            rlp.list[7].size = dump_nlzint(txn.r);
            rlp.list[8].size = dump_nlzint(txn.s);
        }
        for (uint32_t i = 0; i < rlp.size; i++) {
            if (rlp.list[i].size > 0) {
                rlp.list[i].data = new uint8_t[rlp.list[i].size];
                if (rlp.list == nullptr) throw MEMORY_EXAUSTED;
            }
        }
        dump_nlzint(txn.nonce, rlp.list[0].data, rlp.list[0].size);
        dump_nlzint(txn.gasprice, rlp.list[1].data, rlp.list[1].size);
        dump_nlzint(txn.gaslimit, rlp.list[2].data, rlp.list[2].size);
        if (txn.has_to) {
            uint256_t::to(txn.to, rlp.list[3].data, rlp.list[3].size);
        }
        dump_nlzint(txn.value, rlp.list[4].data, rlp.list[4].size);
        for (uint32_t i = 0; i < txn.data_size; i++) rlp.list[5].data[i] = txn.data[i];
        if (txn.is_signed) {
            dump_nlzint(txn.v, rlp.list[6].data, rlp.list[6].size);
            dump_nlzint(txn.r, rlp.list[7].data, rlp.list[7].size);
            dump_nlzint(txn.s, rlp.list[8].data, rlp.list[8].size);
        }
        uint32_t result = dump_rlp(rlp, buffer, size);
        free_rlp(rlp);
        return result;
    } catch (Error e) {
        free_rlp(rlp);
        throw e;
    }
}

static uint32_t encode_txn(const struct txn &txn)
{
    return encode_txn(txn, nullptr, 0);
}

static struct txn decode_txn(const uint8_t *buffer, uint32_t size)
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

static uint32_t encode_cid(const uint256_t &from, const uint256_t &nonce, uint8_t *buffer, uint32_t size)
{
    struct rlp rlp;
    rlp.is_list = true;
    rlp.size = 2;
    rlp.list = new struct rlp[rlp.size];
    if (rlp.list == nullptr) throw MEMORY_EXAUSTED;
    for (uint32_t i = 0; i < rlp.size; i++) {
        rlp.list[i].is_list = false;
        rlp.list[i].size = 0;
        rlp.list[i].data = nullptr;
    }
    try {
        rlp.list[0].size = dump_nlzint(from);
        rlp.list[1].size = dump_nlzint(nonce);
        for (uint32_t i = 0; i < rlp.size; i++) {
            if (rlp.list[i].size > 0) {
                rlp.list[i].data = new uint8_t[rlp.list[i].size];
                if (rlp.list == nullptr) throw MEMORY_EXAUSTED;
            }
        }
        dump_nlzint(from, rlp.list[0].data, rlp.list[0].size);
        dump_nlzint(nonce, rlp.list[1].data, rlp.list[1].size);
        uint32_t result = dump_rlp(rlp, buffer, size);
        free_rlp(rlp);
        return result;
    } catch (Error e) {
        free_rlp(rlp);
        throw e;
    }
}

static uint32_t encode_cid(const uint256_t &from, const uint256_t &nonce)
{
    return encode_cid(from, nonce, nullptr, 0);
}

/* gas */

enum GasType : uint8_t {
    GasNone = 0,
    GasQuickStep,
    GasFastestStep,
    GasFastStep,
    GasMidStep,
    GasSlowStep,
    GasExtStep,
    GasSha3,
    GasBalance,
    GasExtcodeSize,
    GasExtcodeCopy,
    GasExtcodeHash,
    GasSload,
    GasJumpdest,
    GasCreate,
    GasCall,
    GasCreate2,
    GasTxMessageCall,
    GasTxContractCreation,
    GasTxDataZero,
    GasTxDataNonZero,
};

enum GasValue : uint32_t {
    _GasNone = 0,
    _GasQuickStep = 2,
    _GasFastestStep = 3,
    _GasFastStep = 5,
    _GasMidStep = 8,
    _GasSlowStep = 10,
    _GasExtStep = 20,
    _GasSha3 = 30,
    _GasBalance = 20,
    _GasExtcodeSize = 20,
    _GasExtcodeCopy = 20,
    _GasExtcodeHash = 400,
    _GasSload = 50,
    _GasJumpdest = 1,
    _GasCreate = 32000,
    _GasCall = 40,
    _GasCreate2 = 32000,
    _GasTxMessageCall = 21000,
    _GasTxContractCreation = 21000,
    _GasTxDataZero = 4,
    _GasTxDataNonZero = 68,

    _GasTxContractCreation_Homestead = 53000,

    _GasBalance_TangerineWhistle = 400,
    _GasExtcodeSize_TangerineWhistle = 700,
    _GasExtcodeCopy_TangerineWhistle = 700,
    _GasSload_TangerineWhistle = 200,
    _GasCall_TangerineWhistle = 700,

    _GasBalance_Istanbul = 700,
    _GasExtcodeHash_Istanbul = 700,
    _GasSload_Istanbul = 800,
    _GasTxDataNonZero_Istanbul = 16,
};

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

static const GasType constgas[256] = {
    /*STOP*/GasNone, /*ADD*/GasFastestStep, /*MUL*/GasFastStep, /*SUB*/GasFastestStep, /*DIV*/GasFastStep, /*SDIV*/GasFastStep, /*MOD*/GasFastStep, /*SMOD*/GasFastStep,
    /*ADDMOD*/GasMidStep, /*MULMOD*/GasMidStep, /*EXP*/GasNone, /*SIGNEXTEND*/GasFastStep, GasNone, GasNone, GasNone, GasNone,
    /*LT*/GasFastestStep, /*GT*/GasFastestStep, /*SLT*/GasFastestStep, /*SGT*/GasFastestStep, /*EQ*/GasFastestStep, /*ISZERO*/GasFastestStep, /*AND*/GasFastestStep, /*OR*/GasFastestStep,
    /*XOR*/GasFastestStep, /*NOT*/GasFastestStep, /*BYTE*/GasFastestStep, /*SHL*/GasFastestStep, /*SHR*/GasFastestStep, /*SAR*/GasFastestStep, GasNone, GasNone,
    /*SHA3*/GasSha3, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    /*ADDRESS*/GasQuickStep, /*BALANCE*/GasBalance, /*ORIGIN*/GasQuickStep, /*CALLER*/GasQuickStep, /*CALLVALUE*/GasQuickStep, /*CALLDATALOAD*/GasFastestStep, /*CALLDATASIZE*/GasQuickStep, /*CALLDATACOPY*/GasFastestStep,
    /*CODESIZE*/GasQuickStep, /*CODECOPY*/GasFastestStep, /*GASPRICE*/GasQuickStep, /*EXTCODESIZE*/GasExtcodeSize, /*EXTCODECOPY*/GasExtcodeCopy, /*RETURNDATASIZE*/GasQuickStep, /*RETURNDATACOPY*/GasFastestStep, /*EXTCODEHASH*/GasExtcodeHash,
    /*BLOCKHASH*/GasExtStep, /*COINBASE*/GasQuickStep, /*TIMESTAMP*/GasQuickStep, /*NUMBER*/GasQuickStep, /*DIFFICULTY*/GasQuickStep, /*GASLIMIT*/GasQuickStep, /*CHAINID*/GasQuickStep, /*SELFBALANCE*/GasFastStep,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    /*POP*/GasQuickStep, /*MLOAD*/GasFastestStep, /*MSTORE*/GasFastestStep, /*MSTORE8*/GasFastestStep, /*SLOAD*/GasSload, /*SSTORE*/GasNone, /*JUMP*/GasMidStep, /*JUMPI*/GasSlowStep,
    /*PC*/GasQuickStep, /*MSIZE*/GasQuickStep, /*GAS*/GasQuickStep, /*JUMPDEST*/GasJumpdest, GasNone, GasNone, GasNone, GasNone,
    /*PUSH1*/GasFastestStep, /*PUSH2*/GasFastestStep, /*PUSH3*/GasFastestStep, /*PUSH4*/GasFastestStep, /*PUSH5*/GasFastestStep, /*PUSH6*/GasFastestStep, /*PUSH7*/GasFastestStep, /*PUSH8*/GasFastestStep,
    /*PUSH9*/GasFastestStep, /*PUSH10*/GasFastestStep, /*PUSH11*/GasFastestStep, /*PUSH12*/GasFastestStep, /*PUSH13*/GasFastestStep, /*PUSH14*/GasFastestStep, /*PUSH15*/GasFastestStep, /*PUSH16*/GasFastestStep,
    /*PUSH17*/GasFastestStep, /*PUSH18*/GasFastestStep, /*PUSH19*/GasFastestStep, /*PUSH20*/GasFastestStep, /*PUSH21*/GasFastestStep, /*PUSH22*/GasFastestStep, /*PUSH23*/GasFastestStep, /*PUSH24*/GasFastestStep,
    /*PUSH25*/GasFastestStep, /*PUSH26*/GasFastestStep, /*PUSH27*/GasFastestStep, /*PUSH28*/GasFastestStep, /*PUSH29*/GasFastestStep, /*PUSH30*/GasFastestStep, /*PUSH31*/GasFastestStep, /*PUSH32*/GasFastestStep,
    /*DUP1*/GasFastestStep, /*DUP2*/GasFastestStep, /*DUP3*/GasFastestStep, /*DUP4*/GasFastestStep, /*DUP5*/GasFastestStep, /*DUP6*/GasFastestStep, /*DUP7*/GasFastestStep, /*DUP8*/GasFastestStep,
    /*DUP9*/GasFastestStep, /*DUP10*/GasFastestStep, /*DUP11*/GasFastestStep, /*DUP12*/GasFastestStep, /*DUP13*/GasFastestStep, /*DUP14*/GasFastestStep, /*DUP15*/GasFastestStep, /*DUP16*/GasFastestStep,
    /*SWAP1*/GasFastestStep, /*SWAP2*/GasFastestStep, /*SWAP3*/GasFastestStep, /*SWAP4*/GasFastestStep, /*SWAP5*/GasFastestStep, /*SWAP6*/GasFastestStep, /*SWAP7*/GasFastestStep, /*SWAP8*/GasFastestStep,
    /*SWAP9*/GasFastestStep, /*SWAP10*/GasFastestStep, /*SWAP11*/GasFastestStep, /*SWAP12*/GasFastestStep, /*SWAP13*/GasFastestStep, /*SWAP14*/GasFastestStep, /*SWAP15*/GasFastestStep, /*SWAP16*/GasFastestStep,
    /*LOG0*/GasNone, /*LOG1*/GasNone, /*LOG2*/GasNone, /*LOG3*/GasNone, /*LOG4*/GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone, GasNone,
    /*CREATE*/GasCreate, /*CALL*/GasCall, /*CALLCODE*/GasCall, /*RETURN*/GasNone, /*DELEGATECALL*/GasCall, /*CREATE2*/GasCreate2, GasNone, GasNone,
    GasNone, GasNone, /*STATICCALL*/GasCall, GasNone, GasNone, /*REVERT*/GasNone, GasNone, /*SELFDESTRUCT*/GasNone,
};

enum Precompiled : uint8_t {
    ECRECOVER = 0x01,
    SHA256,
    RIPEMD160,
    DATACOPY,
    BIGMODEEXP,
    BN256ADD,
    BN256SCALARMUL,
    BN256PAIRING,
    BLAKE2F,
};

enum Release {
    FRONTIER = 0,
    // FRONTIER_THAWING
    HOMESTEAD,
    // DAO
    TANGERINE_WHISTLE,
    SPURIOUS_DRAGON,
    BYZANTIUM,
    CONSTANTINOPLE,
    // PETERSBURG
    ISTANBUL,
    // MUIR_GLACIER
};

const uint256_t _1 = (uint256_t)1;
const uint256_t is_frontier = 0
    | _1 << STOP | _1 << ADD | _1 << MUL | _1 << SUB | _1 << DIV | _1 << SDIV | _1 << MOD | _1 << SMOD
    | _1 << ADDMOD | _1 << MULMOD | _1 << EXP | _1 << SIGNEXTEND
    | _1 << LT | _1 << GT | _1 << SLT | _1 << SGT | _1 << EQ | _1 << ISZERO | _1 << AND | _1 << OR
    | _1 << XOR | _1 << NOT | _1 << BYTE
    | _1 << SHA3
    | _1 << ADDRESS | _1 << BALANCE | _1 << ORIGIN | _1 << CALLER | _1 << CALLVALUE | _1 << CALLDATALOAD | _1 << CALLDATASIZE | _1 << CALLDATACOPY
    | _1 << CODESIZE | _1 << CODECOPY | _1 << GASPRICE | _1 << EXTCODESIZE | _1 << EXTCODECOPY
    | _1 << BLOCKHASH | _1 << COINBASE | _1 << TIMESTAMP | _1 << NUMBER | _1 << DIFFICULTY | _1 << GASLIMIT
    | _1 << POP | _1 << MLOAD | _1 << MSTORE | _1 << MSTORE8 | _1 << SLOAD | _1 << SSTORE | _1 << JUMP | _1 << JUMPI
    | _1 << PC | _1 << MSIZE | _1 << GAS | _1 << JUMPDEST
    | _1 << PUSH1 | _1 << PUSH2 | _1 << PUSH3 | _1 << PUSH4 | _1 << PUSH5 | _1 << PUSH6 | _1 << PUSH7 | _1 << PUSH8
    | _1 << PUSH9 | _1 << PUSH10 | _1 << PUSH11 | _1 << PUSH12 | _1 << PUSH13 | _1 << PUSH14 | _1 << PUSH15 | _1 << PUSH16
    | _1 << PUSH17 | _1 << PUSH18 | _1 << PUSH19 | _1 << PUSH20 | _1 << PUSH21 | _1 << PUSH22 | _1 << PUSH23 | _1 << PUSH24
    | _1 << PUSH25 | _1 << PUSH26 | _1 << PUSH27 | _1 << PUSH28 | _1 << PUSH29 | _1 << PUSH30 | _1 << PUSH31 | _1 << PUSH32
    | _1 << DUP1 | _1 << DUP2 | _1 << DUP3 | _1 << DUP4 | _1 << DUP5 | _1 << DUP6 | _1 << DUP7 | _1 << DUP8
    | _1 << DUP9 | _1 << DUP10 | _1 << DUP11 | _1 << DUP12 | _1 << DUP13 | _1 << DUP14 | _1 << DUP15 | _1 << DUP16
    | _1 << SWAP1 | _1 << SWAP2 | _1 << SWAP3 | _1 << SWAP4 | _1 << SWAP5 | _1 << SWAP6 | _1 << SWAP7 | _1 << SWAP8
    | _1 << SWAP9 | _1 << SWAP10 | _1 << SWAP11 | _1 << SWAP12 | _1 << SWAP13 | _1 << SWAP14 | _1 << SWAP15 | _1 << SWAP16
    | _1 << LOG0 | _1 << LOG1 | _1 << LOG2 | _1 << LOG3 | _1 << LOG4
    | _1 << CREATE | _1 << CALL | _1 << CALLCODE | _1 << RETURN
    | _1 << SELFDESTRUCT;
const uint256_t is_homestead = is_frontier
    | _1 << DELEGATECALL;
const uint256_t is_tangerine_whistle = is_homestead;
const uint256_t is_spurious_dragon = is_tangerine_whistle;
const uint256_t is_byzantium = is_spurious_dragon
    | _1 << STATICCALL
    | _1 << RETURNDATASIZE
    | _1 << RETURNDATACOPY
    | _1 << REVERT;
const uint256_t is_constantinople = is_byzantium
    | _1 << SHL
    | _1 << SHR
    | _1 << SAR
    | _1 << EXTCODEHASH
    | _1 << CREATE2;
const uint256_t is_istanbul = is_constantinople
    | _1 << CHAINID
    | _1 << SELFBALANCE;

static const uint256_t is[ISTANBUL+1] = {
    is_frontier,
    is_homestead,
    is_tangerine_whistle,
    is_spurious_dragon,
    is_byzantium,
    is_constantinople,
    is_istanbul,
};

static uint256_t pre_frontier = 0;
static uint256_t pre_homestead = pre_frontier
    | 1 << ECRECOVER
    | 1 << SHA256
    | 1 << RIPEMD160
    | 1 << DATACOPY;
static uint256_t pre_tangerine_whistle = pre_homestead;
static uint256_t pre_spurious_dragon = pre_tangerine_whistle;
static uint256_t pre_byzantium = pre_spurious_dragon
    | 1 << BIGMODEEXP
    | 1 << BN256ADD
    | 1 << BN256SCALARMUL
    | 1 << BN256PAIRING;
static uint256_t pre_constantinople = pre_byzantium;
static uint256_t pre_istanbul = pre_constantinople
    | 1 << BLAKE2F;

static const uint256_t pre[ISTANBUL+1] = {
    pre_frontier,
    pre_homestead,
    pre_tangerine_whistle,
    pre_spurious_dragon,
    pre_byzantium,
    pre_constantinople,
    pre_istanbul,
};

const uint256_t is_halts = 0
    | _1 << STOP
    | _1 << RETURN
    | _1 << SELFDESTRUCT;
const uint256_t is_jumps = 0
    | _1 << JUMP | _1 << JUMPI;
const uint256_t is_writes = 0
    | _1 << SSTORE
    | _1 << LOG0 | _1 << LOG1 | _1 << LOG2 | _1 << LOG3 | _1 << LOG4
    | _1 << CREATE | _1 << CREATE2
    | _1 << SELFDESTRUCT;
const uint256_t is_reverts = 0
    | _1 << REVERT;
const uint256_t is_returns = 0
    | _1 << CREATE | _1 << CREATE2
    | _1 << CALL | _1 << CALLCODE | _1 << DELEGATECALL | _1 << STATICCALL
    | _1 << REVERT;

static const uint32_t is_gas_table[4][GasTxDataNonZero+1] = {
    {   // frontier
        _GasNone,
        _GasQuickStep,
        _GasFastestStep,
        _GasFastStep,
        _GasMidStep,
        _GasSlowStep,
        _GasExtStep,
        _GasSha3,
        _GasBalance,
        _GasExtcodeSize,
        _GasExtcodeCopy,
        _GasExtcodeHash,
        _GasSload,
        _GasJumpdest,
        _GasCreate,
        _GasCall,
        _GasCreate2,
        _GasTxMessageCall,
        _GasTxContractCreation,
        _GasTxDataZero,
        _GasTxDataNonZero,
    },
    {   // homestead
        _GasNone,
        _GasQuickStep,
        _GasFastestStep,
        _GasFastStep,
        _GasMidStep,
        _GasSlowStep,
        _GasExtStep,
        _GasSha3,
        _GasBalance,
        _GasExtcodeSize,
        _GasExtcodeCopy,
        _GasExtcodeHash,
        _GasSload,
        _GasJumpdest,
        _GasCreate,
        _GasCall,
        _GasCreate2,
        _GasTxMessageCall,
        _GasTxContractCreation_Homestead,
        _GasTxDataZero,
        _GasTxDataNonZero,
    },
    {   // tangerine whistle | spurious dragon | byzantium | constantinople
        _GasNone,
        _GasQuickStep,
        _GasFastestStep,
        _GasFastStep,
        _GasMidStep,
        _GasSlowStep,
        _GasExtStep,
        _GasSha3,
        _GasBalance_TangerineWhistle,
        _GasExtcodeSize_TangerineWhistle,
        _GasExtcodeCopy_TangerineWhistle,
        _GasExtcodeHash,
        _GasSload_TangerineWhistle,
        _GasJumpdest,
        _GasCreate,
        _GasCall_TangerineWhistle,
        _GasCreate2,
        _GasTxMessageCall,
        _GasTxContractCreation_Homestead,
        _GasTxDataZero,
        _GasTxDataNonZero,
    },
    {   // istanbul
        _GasNone,
        _GasQuickStep,
        _GasFastestStep,
        _GasFastStep,
        _GasMidStep,
        _GasSlowStep,
        _GasExtStep,
        _GasSha3,
        _GasBalance_Istanbul,
        _GasExtcodeSize_TangerineWhistle,
        _GasExtcodeCopy_TangerineWhistle,
        _GasExtcodeHash_Istanbul,
        _GasSload_Istanbul,
        _GasJumpdest,
        _GasCreate,
        _GasCall_TangerineWhistle,
        _GasCreate2,
        _GasTxMessageCall,
        _GasTxContractCreation_Homestead,
        _GasTxDataZero,
        _GasTxDataNonZero_Istanbul,
    },
};
static uint8_t is_gas_index[ISTANBUL+1] = { 0, 1, 2, 2, 2, 2, 3 };

static inline uint32_t _gas(Release release, GasType type)
{
    return is_gas_table[is_gas_index[release]][type];
}

static inline uint32_t opcode_gas(Release release, uint8_t opc)
{
    return _gas(release, constgas[opc]);
}

/*

Dynamic Gas


CALLDATACOPY memoryCopierGas(2)
CODECOPY memoryCopierGas(2)
EXTCODECOPY memoryCopierGas(3)
RETURNDATACOPY memoryCopierGas(2)

MLOAD pureMemoryGascost
MSTORE pureMemoryGascost
MSTORE8 pureMemoryGascost
CREATE pureMemoryGascost
RETURN pureMemoryGascost
REVERT pureMemoryGascost

LOG0 makeGasLog(0)
LOG1 makeGasLog(1)
LOG2 makeGasLog(2)
LOG3 makeGasLog(3)
LOG4 makeGasLog(4)

SHA3 gasSHA3
CREATE2 gasCreate2
CALL gasCall
CALLCODE gasCallCode
SELFDESTRUCT gasSelfdestruct
DELEGATECALL gasDelegateCall
STATICCALL gasStaticCall

EXP gasExpFrontier
TangerineWhistle -> SpuriousDragon EXP gasExpEIP158

SSTORE gasSStore
Constantinople -> Istanbul SSTORE gasSStoreEIP2200

func gasExpEIP158(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	expByteLen := uint64((stack.data[stack.len()-2].BitLen() + 7) / 8)

	var (
		gas      = expByteLen * params.ExpByteEIP158 // no overflow check required. Max is 256 * ExpByte gas
		overflow bool
	)
	if gas, overflow = math.SafeAdd(gas, params.ExpGas); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func gasSStoreEIP2200(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	// If we fail the minimum gas availability invariant, fail (0)
	if contract.Gas <= params.SstoreSentryGasEIP2200 {
		return 0, errors.New("not enough gas for reentrancy sentry")
	}
	// Gas sentry honoured, do the actual gas calculation based on the stored value
	var (
		y, x    = stack.Back(1), stack.Back(0)
		current = evm.StateDB.GetState(contract.Address(), common.BigToHash(x))
	)
	value := common.BigToHash(y)

	if current == value { // noop (1)
		return params.SstoreNoopGasEIP2200, nil
	}
	original := evm.StateDB.GetCommittedState(contract.Address(), common.BigToHash(x))
	if original == current {
		if original == (common.Hash{}) { // create slot (2.1.1)
			return params.SstoreInitGasEIP2200, nil
		}
		if value == (common.Hash{}) { // delete slot (2.1.2b)
			evm.StateDB.AddRefund(params.SstoreClearRefundEIP2200)
		}
		return params.SstoreCleanGasEIP2200, nil // write existing slot (2.1.2)
	}
	if original != (common.Hash{}) {
		if current == (common.Hash{}) { // recreate slot (2.2.1.1)
			evm.StateDB.SubRefund(params.SstoreClearRefundEIP2200)
		} else if value == (common.Hash{}) { // delete slot (2.2.1.2)
			evm.StateDB.AddRefund(params.SstoreClearRefundEIP2200)
		}
	}
	if original == value {
		if original == (common.Hash{}) { // reset to original inexistent slot (2.2.2.1)
			evm.StateDB.AddRefund(params.SstoreInitRefundEIP2200)
		} else { // reset to original existing slot (2.2.2.2)
			evm.StateDB.AddRefund(params.SstoreCleanRefundEIP2200)
		}
	}
	return params.SstoreDirtyGasEIP2200, nil // dirty update (2.2)
}

func memoryGasCost(mem *Memory, newMemSize uint64) (uint64, error) {
	if newMemSize == 0 {
		return 0, nil
	}
	// The maximum that will fit in a uint64 is max_word_count - 1. Anything above
	// that will result in an overflow. Additionally, a newMemSize which results in
	// a newMemSizeWords larger than 0xFFFFFFFF will cause the square operation to
	// overflow. The constant 0x1FFFFFFFE0 is the highest number that can be used
	// without overflowing the gas calculation.
	if newMemSize > 0x1FFFFFFFE0 {
		return 0, errGasUintOverflow
	}
	newMemSizeWords := toWordSize(newMemSize)
	newMemSize = newMemSizeWords * 32

	if newMemSize > uint64(mem.Len()) {
		square := newMemSizeWords * newMemSizeWords
		linCoef := newMemSizeWords * params.MemoryGas
		quadCoef := square / params.QuadCoeffDiv
		newTotalFee := linCoef + quadCoef

		fee := newTotalFee - mem.lastGasCost
		mem.lastGasCost = newTotalFee

		return fee, nil
	}
	return 0, nil
}

func gasSStore(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	var (
		y, x    = stack.Back(1), stack.Back(0)
		current = evm.StateDB.GetState(contract.Address(), common.BigToHash(x))
	)
	// The legacy gas metering only takes into consideration the current state
	// Legacy rules should be applied if we are in Petersburg (removal of EIP-1283)
	// OR Constantinople is not active
	if evm.chainRules.IsPetersburg || !evm.chainRules.IsConstantinople {
		// This checks for 3 scenario's and calculates gas accordingly:
		//
		// 1. From a zero-value address to a non-zero value         (NEW VALUE)
		// 2. From a non-zero value address to a zero-value address (DELETE)
		// 3. From a non-zero to a non-zero                         (CHANGE)
		switch {
		case current == (common.Hash{}) && y.Sign() != 0: // 0 => non 0
			return params.SstoreSetGas, nil
		case current != (common.Hash{}) && y.Sign() == 0: // non 0 => 0
			evm.StateDB.AddRefund(params.SstoreRefundGas)
			return params.SstoreClearGas, nil
		default: // non 0 => non 0 (or 0 => 0)
			return params.SstoreResetGas, nil
		}
	}
	// The new gas metering is based on net gas costs (EIP-1283):
	//
	// 1. If current value equals new value (this is a no-op), 200 gas is deducted.
	// 2. If current value does not equal new value
	//   2.1. If original value equals current value (this storage slot has not been changed by the current execution context)
	//     2.1.1. If original value is 0, 20000 gas is deducted.
	// 	   2.1.2. Otherwise, 5000 gas is deducted. If new value is 0, add 15000 gas to refund counter.
	// 	2.2. If original value does not equal current value (this storage slot is dirty), 200 gas is deducted. Apply both of the following clauses.
	// 	  2.2.1. If original value is not 0
	//       2.2.1.1. If current value is 0 (also means that new value is not 0), remove 15000 gas from refund counter. We can prove that refund counter will never go below 0.
	//       2.2.1.2. If new value is 0 (also means that current value is not 0), add 15000 gas to refund counter.
	// 	  2.2.2. If original value equals new value (this storage slot is reset)
	//       2.2.2.1. If original value is 0, add 19800 gas to refund counter.
	// 	     2.2.2.2. Otherwise, add 4800 gas to refund counter.
	value := common.BigToHash(y)
	if current == value { // noop (1)
		return params.NetSstoreNoopGas, nil
	}
	original := evm.StateDB.GetCommittedState(contract.Address(), common.BigToHash(x))
	if original == current {
		if original == (common.Hash{}) { // create slot (2.1.1)
			return params.NetSstoreInitGas, nil
		}
		if value == (common.Hash{}) { // delete slot (2.1.2b)
			evm.StateDB.AddRefund(params.NetSstoreClearRefund)
		}
		return params.NetSstoreCleanGas, nil // write existing slot (2.1.2)
	}
	if original != (common.Hash{}) {
		if current == (common.Hash{}) { // recreate slot (2.2.1.1)
			evm.StateDB.SubRefund(params.NetSstoreClearRefund)
		} else if value == (common.Hash{}) { // delete slot (2.2.1.2)
			evm.StateDB.AddRefund(params.NetSstoreClearRefund)
		}
	}
	if original == value {
		if original == (common.Hash{}) { // reset to original inexistent slot (2.2.2.1)
			evm.StateDB.AddRefund(params.NetSstoreResetClearRefund)
		} else { // reset to original existing slot (2.2.2.2)
			evm.StateDB.AddRefund(params.NetSstoreResetRefund)
		}
	}
	return params.NetSstoreDirtyGas, nil
}

func gasCall(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	var (
		gas            uint64
		transfersValue = stack.Back(2).Sign() != 0
		address        = common.BigToAddress(stack.Back(1))
	)
	if evm.chainRules.IsEIP158 {
		if transfersValue && evm.StateDB.Empty(address) {
			gas += params.CallNewAccountGas
		}
	} else if !evm.StateDB.Exist(address) {
		gas += params.CallNewAccountGas
	}
	if transfersValue {
		gas += params.CallValueTransferGas
	}
	memoryGas, err := memoryGasCost(mem, memorySize)
	if err != nil {
		return 0, err
	}
	var overflow bool
	if gas, overflow = math.SafeAdd(gas, memoryGas); overflow {
		return 0, errGasUintOverflow
	}

	evm.callGasTemp, err = callGas(evm.chainRules.IsEIP150, contract.Gas, gas, stack.Back(0))
	if err != nil {
		return 0, err
	}
	if gas, overflow = math.SafeAdd(gas, evm.callGasTemp); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func gasCallCode(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	memoryGas, err := memoryGasCost(mem, memorySize)
	if err != nil {
		return 0, err
	}
	var (
		gas      uint64
		overflow bool
	)
	if stack.Back(2).Sign() != 0 {
		gas += params.CallValueTransferGas
	}
	if gas, overflow = math.SafeAdd(gas, memoryGas); overflow {
		return 0, errGasUintOverflow
	}
	evm.callGasTemp, err = callGas(evm.chainRules.IsEIP150, contract.Gas, gas, stack.Back(0))
	if err != nil {
		return 0, err
	}
	if gas, overflow = math.SafeAdd(gas, evm.callGasTemp); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func gasDelegateCall(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	gas, err := memoryGasCost(mem, memorySize)
	if err != nil {
		return 0, err
	}
	evm.callGasTemp, err = callGas(evm.chainRules.IsEIP150, contract.Gas, gas, stack.Back(0))
	if err != nil {
		return 0, err
	}
	var overflow bool
	if gas, overflow = math.SafeAdd(gas, evm.callGasTemp); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func gasStaticCall(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	gas, err := memoryGasCost(mem, memorySize)
	if err != nil {
		return 0, err
	}
	evm.callGasTemp, err = callGas(evm.chainRules.IsEIP150, contract.Gas, gas, stack.Back(0))
	if err != nil {
		return 0, err
	}
	var overflow bool
	if gas, overflow = math.SafeAdd(gas, evm.callGasTemp); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func gasSelfdestruct(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	var gas uint64
	// EIP150 homestead gas reprice fork:
	if evm.chainRules.IsEIP150 {
		gas = params.SelfdestructGasEIP150
		var address = common.BigToAddress(stack.Back(0))

		if evm.chainRules.IsEIP158 {
			// if empty and transfers value
			if evm.StateDB.Empty(address) && evm.StateDB.GetBalance(contract.Address()).Sign() != 0 {
				gas += params.CreateBySelfdestructGas
			}
		} else if !evm.StateDB.Exist(address) {
			gas += params.CreateBySelfdestructGas
		}
	}

	if !evm.StateDB.HasSuicided(contract.Address()) {
		evm.StateDB.AddRefund(params.SelfdestructRefundGas)
	}
	return gas, nil
}

func gasSha3(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	gas, err := memoryGasCost(mem, memorySize)
	if err != nil {
		return 0, err
	}
	wordGas, overflow := bigUint64(stack.Back(1))
	if overflow {
		return 0, errGasUintOverflow
	}
	if wordGas, overflow = math.SafeMul(toWordSize(wordGas), params.Sha3WordGas); overflow {
		return 0, errGasUintOverflow
	}
	if gas, overflow = math.SafeAdd(gas, wordGas); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func gasCreate2(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	gas, err := memoryGasCost(mem, memorySize)
	if err != nil {
		return 0, err
	}
	wordGas, overflow := bigUint64(stack.Back(2))
	if overflow {
		return 0, errGasUintOverflow
	}
	if wordGas, overflow = math.SafeMul(toWordSize(wordGas), params.Sha3WordGas); overflow {
		return 0, errGasUintOverflow
	}
	if gas, overflow = math.SafeAdd(gas, wordGas); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func pureMemoryGascost(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	return memoryGasCost(mem, memorySize)
}

func makeGasLog(n uint64) gasFunc {
	return func(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
		requestedSize, overflow := bigUint64(stack.Back(1))
		if overflow {
			return 0, errGasUintOverflow
		}

		gas, err := memoryGasCost(mem, memorySize)
		if err != nil {
			return 0, err
		}

		if gas, overflow = math.SafeAdd(gas, params.LogGas); overflow {
			return 0, errGasUintOverflow
		}
		if gas, overflow = math.SafeAdd(gas, n*params.LogTopicGas); overflow {
			return 0, errGasUintOverflow
		}

		var memorySizeGas uint64
		if memorySizeGas, overflow = math.SafeMul(requestedSize, params.LogDataGas); overflow {
			return 0, errGasUintOverflow
		}
		if gas, overflow = math.SafeAdd(gas, memorySizeGas); overflow {
			return 0, errGasUintOverflow
		}
		return gas, nil
	}
}

func gasExpFrontier(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
	expByteLen := uint64((stack.data[stack.len()-2].BitLen() + 7) / 8)

	var (
		gas      = expByteLen * params.ExpByteFrontier // no overflow check required. Max is 256 * ExpByte gas
		overflow bool
	)
	if gas, overflow = math.SafeAdd(gas, params.ExpGas); overflow {
		return 0, errGasUintOverflow
	}
	return gas, nil
}

func memoryCopierGas(stackpos int) gasFunc {
	return func(evm *EVM, contract *Contract, stack *Stack, mem *Memory, memorySize uint64) (uint64, error) {
		// Gas for expanding the memory
		gas, err := memoryGasCost(mem, memorySize)
		if err != nil {
			return 0, err
		}
		// And gas for copying data, charged per word at param.CopyGas
		words, overflow := bigUint64(stack.Back(stackpos))
		if overflow {
			return 0, errGasUintOverflow
		}

		if words, overflow = math.SafeMul(toWordSize(words), params.CopyGas); overflow {
			return 0, errGasUintOverflow
		}

		if gas, overflow = math.SafeAdd(gas, words); overflow {
			return 0, errGasUintOverflow
		}
		return gas, nil
	}
}

*/

class Stack {
private:
    static constexpr int L = 1024;
    uint256_t data[L];
    int top = 0;
public:
    inline const uint256_t pop() { if (top == 0) throw STACK_UNDERFLOW;  return data[--top]; }
    inline void push(const uint256_t& v) { if (top == L) throw STACK_OVERFLOW; data[top++] = v; }
    inline uint256_t operator[](int i) const {
        if (i < -top || i >= top) throw OUTOFBOUND_INDEX;
        return data[i < 0 ? top - i: i];
    }
    inline uint256_t& operator[](int i) {
        if (i < -top || i >= top) throw OUTOFBOUND_INDEX;
        return data[i < 0 ? top - i: i];
    }
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
        uint256_t::to(v, buffer);
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
    inline void clear(uint32_t offset, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            (*this)[offset+i] = 0;
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
    virtual const struct account *find_account(const uint256_t &address) = 0;
    virtual void update(const uint256_t &account, const uint256_t &nonce, const uint256_t &balance) = 0;
public:
    inline uint256_t nonce(const uint256_t &v) {
        const struct account *account = find_account(v);
        return account == nullptr ? 0 : account->nonce;
    }
    inline uint256_t balance(const uint256_t &v) {
        const struct account *account = find_account(v);
        return account == nullptr ? 0 : account->balance;
    }
    inline const uint8_t* code(const uint256_t &v) {
        const struct account *account = find_account(v);
        return account == nullptr ? nullptr : account->code;
    }
    inline uint32_t code_size(const uint256_t &v) {
        const struct account *account = find_account(v);
        return account == nullptr ? 0 : account->code_size;
    }
    inline uint256_t code_hash(const uint256_t &v) {
        const struct account *account = find_account(v);
        return account == nullptr ? 0 : account->code_hash;
    }
    virtual void register_code(const uint256_t &account, const uint8_t *buffer, uint32_t size) = 0;
    virtual const uint256_t& load(const uint256_t &account, const uint256_t &address) = 0;
    virtual void store(const uint256_t &account, const uint256_t &address, const uint256_t& v) = 0;
    virtual void log0(const uint256_t &owner, const uint8_t *buffer, uint32_t size) = 0;
    virtual void log1(const uint256_t &owner, const uint256_t &v1, const uint8_t *buffer, uint32_t size) = 0;
    virtual void log2(const uint256_t &owner, const uint256_t &v1, const uint256_t &v2, const uint8_t *buffer, uint32_t size) = 0;
    virtual void log3(const uint256_t &owner, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint8_t *buffer, uint32_t size) = 0;
    virtual void log4(const uint256_t &owner, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint256_t &v4, const uint8_t *buffer, uint32_t size) = 0;
    virtual uint32_t commit() = 0;
    virtual void rollback(uint32_t commit_id) = 0;
    inline void increment_nonce(const uint256_t &v) {
        const struct account *account = find_account(v);
        uint256_t nonce = account == nullptr ? 0 : account->nonce;
        uint256_t balance = account == nullptr ? 0 : account->balance;
        update(v, nonce + 1, balance);
    }
    inline void add_balance(const uint256_t &v, uint256_t amount) {
        const struct account *account = find_account(v);
        uint256_t nonce = account == nullptr ? 0 : account->nonce;
        uint256_t balance = account == nullptr ? 0 : account->balance;
        update(v, nonce, balance + amount);
    }
    inline void sub_balance(const uint256_t &v, uint256_t amount) {
        const struct account *account = find_account(v);
        uint256_t nonce = account == nullptr ? 0 : account->nonce;
        uint256_t balance = account == nullptr ? 0 : account->balance;
        if (amount > account->balance) throw INSUFFICIENT_BALANCE;
        update(v, nonce, balance - amount);
    }
};

class Block {
public:
    virtual uint32_t chainid() = 0;
    virtual uint32_t timestamp() = 0;
    virtual uint32_t number() = 0;
    virtual const uint256_t& coinbase() = 0;
    virtual const uint256_t& gaslimit() = 0;
    virtual const uint256_t& difficulty() = 0;
    virtual uint256_t hash(uint32_t number) = 0;
};

static inline uint160_t gen_address(const uint256_t &from, const uint256_t &nonce)
{
    uint32_t size = encode_cid(from, nonce);
    uint8_t buffer[size];
    encode_cid(from, nonce, buffer, size);
    return (uint160_t)sha3(buffer, size);
}

static inline uint160_t gen_address(const uint256_t &from, const uint256_t &salt, const uint256_t &hash)
{
    uint32_t size = 1 + 20 + 32 + 32;
    uint8_t buffer[size];
    uint32_t offset = 0;
    buffer[offset] = 0xff; offset += 1;
    uint160_t::to((uint160_t)from, &buffer[offset]); offset += 20;
    uint256_t::to(salt, &buffer[offset]); offset += 32;
    uint256_t::to(hash, &buffer[offset]); offset += 32;
    return (uint160_t)sha3(buffer, size);
}

static inline uint32_t _min(uint32_t v1, uint32_t v2) { return v1 < v2 ? v1 : v2;}

static inline void _ensure_capacity(uint8_t *&data, uint32_t &size, uint32_t &capacity)
{
    if (size > capacity) {
        uint8_t *buffer = new uint8_t[size];
        if (buffer == nullptr) throw MEMORY_EXAUSTED;
        delete data;
        data = buffer;
        capacity = size;
    }
}

static bool vm_run(const Release release, Block &block, Storage &storage,
    const uint256_t &origin_address, const uint256_t &gas_price,
    const uint256_t &owner_address, const uint8_t *code, const uint32_t code_size,
    const uint256_t &caller_address, const uint256_t &call_value, const uint8_t *call_data, const uint32_t call_size,
    uint8_t *&return_data, uint32_t &return_size, uint32_t &return_capacity, uint256_t &gas,
    bool read_only, uint32_t depth)
{
    if (depth > 1024) throw RECURSION_LIMITED;
    if (code_size == 0) {
        if ((intptr_t)code < 256) {
            uint8_t opc = (intptr_t)code;
            if ((pre[release] & (1 << opc)) == 0) {
                switch (opc) {
                case ECRECOVER: {
                    uint32_t size = 32 + 32 + 32 + 32;
                    uint8_t buffer[size];
                    uint32_t minsize = _min(size, call_size);
                    for (uint32_t i = 0; i < minsize; i++) buffer[i] = call_data[i];
                    for (uint32_t i = minsize; i < size; i++) buffer[i] = 0;
                    uint32_t offset = 0;
                    uint256_t h = uint256_t::from(&buffer[offset]); offset += 32;
                    uint256_t v = uint256_t::from(&buffer[offset]); offset += 32;
                    uint256_t r = uint256_t::from(&buffer[offset]); offset += 32;
                    uint256_t s = uint256_t::from(&buffer[offset]); offset += 32;
                    uint256_t address = ecrecover(h, v, r, s);
                    return_size = 32;
                    _ensure_capacity(return_data, return_size, return_capacity);
                    uint256_t::to(address, return_data, return_size);
                    return true;
                }
                case SHA256: {
                    uint256_t hash = sha256(call_data, call_size);
                    return_size = 32;
                    _ensure_capacity(return_data, return_size, return_capacity);
                    uint256_t::to(hash, return_data, return_size);
                    return true;
                }
                case RIPEMD160: {
                    uint256_t hash = (uint256_t)ripemd160(call_data, call_size);
                    return_size = 32;
                    _ensure_capacity(return_data, return_size, return_capacity);
                    uint256_t::to(hash, return_data, return_size);
                    return true;
                }
                case DATACOPY: {
                    return_size = call_size;
                    _ensure_capacity(return_data, return_size, return_capacity);
                    for (uint32_t i = 0; i < return_size; i++) return_data[i] = call_data[i];
                    return true;
                }
                case BIGMODEEXP: throw UNIMPLEMENTED;
                case BN256ADD: {
                    if (call_size != 4 * 32) throw INVALID_SIZE;
                    uint32_t call_offset = 0;
                    uint256_t x1 = uint256_t::from(&call_data[call_offset]); call_offset += 32;
                    uint256_t y1 = uint256_t::from(&call_data[call_offset]); call_offset += 32;
                    uint256_t x2 = uint256_t::from(&call_data[call_offset]); call_offset += 32;
                    uint256_t y2 = uint256_t::from(&call_data[call_offset]); call_offset += 32;
                    if (mod_t(x1).as256() != x1) throw INVALID_ENCODING;
                    if (mod_t(x2).as256() != x2) throw INVALID_ENCODING;
                    if (mod_t(y1).as256() != y1) throw INVALID_ENCODING;
                    if (mod_t(y2).as256() != y2) throw INVALID_ENCODING;
                    point_bn_t p1 = point_bn_t(x1, y1);
                    point_bn_t p2 = point_bn_t(x2, y2);
                    if (x1 == 0 && y1 == 0) p1 = point_bn_t::inf();
                    else if (!p1.belongs()) throw INVALID_ENCODING;
                    if (x2 == 0 && y2 == 0) p2 = point_bn_t::inf();
                    else if (!p2.belongs()) throw INVALID_ENCODING;
                    point_bn_t p3 = p1 + p2;
                    return_size = 2 * 32;
                    _ensure_capacity(return_data, return_size, return_capacity);
                    uint512_t::to(p3.as512(), return_data);
                    return true;
                }
                case BN256SCALARMUL: throw UNIMPLEMENTED;
                case BN256PAIRING: throw UNIMPLEMENTED;
                case BLAKE2F: {
                    if (call_size != 4 + 8 * 8 + 16 * 8 + 2 * 8 + 1) throw INVALID_SIZE;
                    if (call_data[call_size-1] > 1) throw INVALID_ENCODING;
                    uint32_t call_offset = 0;
                    uint32_t rounds = b2w32be(&call_data[call_offset]); call_offset += 4;
                    uint64_t h0 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t h1 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t h2 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t h3 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t h4 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t h5 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t h6 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t h7 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t w[16];
                    for (int i = 0; i < 16; i++) { w[i] = b2w64le(&call_data[call_offset]); call_offset += 8; }
                    uint64_t t0 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    uint64_t t1 = b2w64le(&call_data[call_offset]); call_offset += 8;
                    bool last_chunk = call_data[call_size-1] > 0;
                    blake2f(rounds,
                        h0, h1, h2, h3,
                        h4, h5, h6, h7,
                        w, t0, t1, last_chunk);
                    return_size = 8 * 8;
                    _ensure_capacity(return_data, return_size, return_capacity);
                    uint32_t return_offset = 0;
                    w2b64le(h0, &return_data[return_offset]); return_offset += 8;
                    w2b64le(h1, &return_data[return_offset]); return_offset += 8;
                    w2b64le(h2, &return_data[return_offset]); return_offset += 8;
                    w2b64le(h3, &return_data[return_offset]); return_offset += 8;
                    w2b64le(h4, &return_data[return_offset]); return_offset += 8;
                    w2b64le(h5, &return_data[return_offset]); return_offset += 8;
                    w2b64le(h6, &return_data[return_offset]); return_offset += 8;
                    w2b64le(h7, &return_data[return_offset]); return_offset += 8;
                    return true;
                }
                default: break;
                }
            }
        }
        return_size = 0;
        return true;
    }
    return_size = 0;
    Stack stack;
    Memory memory;
    for (uint32_t pc = 0; ; pc++) {
        uint8_t opc = pc < code_size ? code[pc] : STOP;
        std::cout << opcodes[opc] << std::endl;
        if ((is[release] & (1 << opc)) == 0) throw INVALID_OPCODE;
        if (read_only && (is_writes & (1 << opc)) > 0) throw ILLEGAL_UPDATE;

        uint256_t cost = opcode_gas(release, opc);
        if (cost > gas) throw GAS_EXAUSTED;
        gas -= cost;

        switch (opc) {
        case STOP: { return_size = 0; return true; }
        case ADD: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 + v2); break; }
        case MUL: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 * v2); break; }
        case SUB: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 - v2); break; }
        case DIV: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v2 == 0 ? 0 : v1 / v2); break; }
        case SDIV: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            bool is_neg1 = (v1[0] & 0x80) > 0;
            bool is_neg2 = (v2[0] & 0x80) > 0;
            if (is_neg1) v1 = -v1;
            if (is_neg2) v2 = -v2;
            uint256_t v3 = v2 == 0 ? 0 : v1 / v2;
            if (is_neg1 != is_neg2) v3 = -v3;
            stack.push(v3);
            break;
        }
        case MOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v2 == 0 ? 0 : v1 % v2); break; }
        case SMOD: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            bool is_neg1 = (v1[0] & 0x80) > 0;
            bool is_neg2 = (v2[0] & 0x80) > 0;
            if (is_neg1) v1 = -v1;
            if (is_neg2) v2 = -v2;
            uint256_t v3 = v2 == 0 ? 0 : v1 % v2;
            if (is_neg1) v3 = -v3;
            stack.push(v3);
            break;
        }
        case ADDMOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(); stack.push(v3 == 0 ? 0 : uint256_t::addmod(v1, v2, v3)); break; }
        case MULMOD: { uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(); stack.push(v3 == 0 ? 0 : uint256_t::mulmod(v1, v2, v3)); break; }
        case EXP: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(uint256_t::pow(v1, v2)); break; }
        case SIGNEXTEND: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 < 32 ? v2.sigext(v1.cast32()) : v2); break; }
        case LT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 < v2); break; }
        case GT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 > v2); break; }
        case SLT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1.sigflip() < v2.sigflip()); break; }
        case SGT: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1.sigflip() > v2.sigflip()); break; }
        case EQ: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 == v2); break; }
        case ISZERO: { uint256_t v1 = stack.pop(); stack.push(v1 == 0); break; }
        case AND: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 & v2); break; }
        case OR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 | v2); break; }
        case XOR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 ^ v2); break; }
        case NOT: { uint256_t v1 = stack.pop(); stack.push(~v1); break; }
        case BYTE: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 < 32 ? v2[v1.cast32()] : 0); break; }
        case SHL: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 > 255 ? 0 : v2 << v1.cast32()); break; }
        case SHR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 > 255 ? 0 : v2 >> v1.cast32()); break; }
        case SAR: { uint256_t v1 = stack.pop(), v2 = stack.pop(); stack.push(v1 > 255 ? ((v2[0] & 0x80) > 0 ? ~(uint256_t)0 : 0) : uint256_t::sar(v2, v1.cast32())); break; }
        case SHA3: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            stack.push(sha3(buffer, size));
            break;
        }
        case ADDRESS: { stack.push(owner_address); break; }
        case BALANCE: { uint256_t v1 = stack.pop(); stack.push(storage.balance(v1)); break; }
        case ORIGIN: { stack.push(origin_address); break; }
        case CALLER: { stack.push(caller_address); break; }
        case CALLVALUE: { stack.push(call_value); break; }
        case CALLDATALOAD: {
            uint256_t v1 = stack.pop();
            uint32_t offset = v1.cast32();
            uint8_t buffer[32];
            uint32_t size = 32;
            if (offset > call_size) offset = call_size;
            if (offset + size > call_size) size = call_size - offset;
            for (uint32_t i = 0; i < size; i++) buffer[i] = call_data[offset + i];
            for (uint32_t i = size; i < 32; i++) buffer[i] = 0;
            stack.push(uint256_t::from(buffer));
            break;
        }
        case CALLDATASIZE: { stack.push(call_size); break; }
        case CALLDATACOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset1 = v1.cast32();
            uint32_t offset2 = v2.cast32();
            uint32_t size = v3.cast32();
            uint32_t _size = size;
            if (offset2 > call_size) offset2 = call_size;
            if (offset2 + size > call_size) size = call_size - offset2;
            memory.burn(offset1, size, &call_data[offset2]);
            memory.clear(offset1 + size, _size - size);
            break;
        }
        case CODESIZE: { stack.push(code_size); break; }
        case CODECOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset1 = v1.cast32();
            uint32_t offset2 = v2.cast32();
            uint32_t size = v3.cast32();
            uint32_t _size = size;
            if (offset2 > code_size) offset2 = code_size;
            if (offset2 + size > code_size) size = code_size - offset2;
            memory.burn(offset1, size, &code[offset2]);
            memory.clear(offset1 + size, _size - size);
            break;
        }
        case GASPRICE: { stack.push(gas_price); break; }
        case EXTCODESIZE: { uint256_t v1 = stack.pop(); stack.push(storage.code_size(v1)); break; }
        case EXTCODECOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t address = v1.cast32();
            uint32_t offset1 = v2.cast32();
            uint32_t offset2 = v3.cast32();
            uint32_t size = v4.cast32();
            const uint8_t *code = storage.code(address);
            const uint32_t code_size = storage.code_size(address);
            uint32_t _size = size;
            if (offset2 > code_size) offset2 = code_size;
            if (offset2 + size > code_size) size = code_size - offset2;
            memory.burn(offset1, size, &code[offset2]);
            memory.clear(offset1 + size, _size - size);
            break;
        }
        case RETURNDATASIZE: { stack.push(return_size); break; }
        case RETURNDATACOPY: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset1 = v1.cast32();
            uint32_t offset2 = v2.cast32();
            uint32_t size = v3.cast32();
            if (offset2 + size > return_size) throw OUTOFBOUND_INDEX;
            memory.burn(offset1, size, &return_data[offset2]);
            break;
        }
        case EXTCODEHASH: { uint256_t v1 = stack.pop(); stack.push(storage.code_hash(v1)); break; }
        case BLOCKHASH: { uint256_t v1 = stack.pop(); stack.push(v1 < block.number()-256 || v1 >= block.number() ? 0 : block.hash(v1.cast32())); break; }
        case COINBASE: { stack.push(block.coinbase()); break; }
        case TIMESTAMP: { stack.push(block.timestamp()); break; }
        case NUMBER: { stack.push(block.number()); break; }
        case DIFFICULTY: { stack.push(block.difficulty()); break; }
        case GASLIMIT: { stack.push(block.gaslimit()); break; }
        case CHAINID: { stack.push(block.chainid()); break; }
        case SELFBALANCE: { stack.push(storage.balance(owner_address)); break; }
        case POP: { stack.pop(); break; }
        case MLOAD: { uint256_t v1 = stack.pop(); stack.push(memory.load(v1.cast32())); break; }
        case MSTORE: { uint256_t v1 = stack.pop(), v2 = stack.pop(); memory.store(v1.cast32(), v2); break; }
        case MSTORE8: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint8_t buffer[1];
            buffer[0] = v2.cast32() & 0xff;
            memory.burn(v1.cast32(), 1, buffer);
            break;
        }
        case SLOAD: { uint256_t v1 = stack.pop(); stack.push(storage.load(owner_address, v1)); break; }
        case SSTORE: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            storage.store(owner_address, v1, v2);
            break;
        }
        case JUMP: {
            uint256_t v1 = stack.pop();
            pc = v1.cast32();
            uint8_t opc = pc < code_size ? code[pc] : STOP;
            if (opc != JUMPDEST) throw ILLEGAL_TARGET;
            break;
        }
        case JUMPI: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            if (v1 != 0) {
                pc = v2.cast32();
                uint8_t opc = pc < code_size ? code[pc] : STOP;
                if (opc != JUMPDEST) throw ILLEGAL_TARGET;
                break;
            }
            break;
        }
        case PC: { stack.push(pc); break; }
        case MSIZE: { stack.push(memory.size()); break; }
        case GAS: { stack.push(gas); break; }
        case JUMPDEST: { break; }
        case PUSH1: { const int n = 1; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH2: { const int n = 2; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH3: { const int n = 3; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH4: { const int n = 4; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH5: { const int n = 5; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH6: { const int n = 6; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH7: { const int n = 7; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH8: { const int n = 8; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH9: { const int n = 9; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH10: { const int n = 10; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH11: { const int n = 11; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH12: { const int n = 12; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH13: { const int n = 13; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH14: { const int n = 14; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH15: { const int n = 15; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH16: { const int n = 16; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH17: { const int n = 17; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH18: { const int n = 18; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH19: { const int n = 19; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH20: { const int n = 20; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH21: { const int n = 21; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH22: { const int n = 22; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH23: { const int n = 23; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH24: { const int n = 24; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH25: { const int n = 25; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH26: { const int n = 26; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH27: { const int n = 27; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH28: { const int n = 28; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH29: { const int n = 29; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH30: { const int n = 30; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH31: { const int n = 31; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case PUSH32: { const int n = 32; uint256_t v1 = uint256_t::from(&code[pc+1], _min(n, code_size - (pc + 1))); stack.push(v1); pc += n; break; }
        case DUP1: { uint256_t v1 = stack[-1]; stack.push(v1); break; }
        case DUP2: { uint256_t v1 = stack[-2]; stack.push(v1); break; }
        case DUP3: { uint256_t v1 = stack[-3]; stack.push(v1); break; }
        case DUP4: { uint256_t v1 = stack[-4]; stack.push(v1); break; }
        case DUP5: { uint256_t v1 = stack[-5]; stack.push(v1); break; }
        case DUP6: { uint256_t v1 = stack[-6]; stack.push(v1); break; }
        case DUP7: { uint256_t v1 = stack[-7]; stack.push(v1); break; }
        case DUP8: { uint256_t v1 = stack[-8]; stack.push(v1); break; }
        case DUP9: { uint256_t v1 = stack[-9]; stack.push(v1); break; }
        case DUP10: { uint256_t v1 = stack[-10]; stack.push(v1); break; }
        case DUP11: { uint256_t v1 = stack[-11]; stack.push(v1); break; }
        case DUP12: { uint256_t v1 = stack[-12]; stack.push(v1); break; }
        case DUP13: { uint256_t v1 = stack[-13]; stack.push(v1); break; }
        case DUP14: { uint256_t v1 = stack[-14]; stack.push(v1); break; }
        case DUP15: { uint256_t v1 = stack[-15]; stack.push(v1); break; }
        case DUP16: { uint256_t v1 = stack[-16]; stack.push(v1); break; }
        case SWAP1: { uint256_t v1 = stack[-1]; stack[-1] = stack[-2]; stack[-2] = v1; break; }
        case SWAP2: { uint256_t v1 = stack[-1]; stack[-1] = stack[-3]; stack[-3] = v1; break; }
        case SWAP3: { uint256_t v1 = stack[-1]; stack[-1] = stack[-4]; stack[-4] = v1; break; }
        case SWAP4: { uint256_t v1 = stack[-1]; stack[-1] = stack[-5]; stack[-5] = v1; break; }
        case SWAP5: { uint256_t v1 = stack[-1]; stack[-1] = stack[-6]; stack[-6] = v1; break; }
        case SWAP6: { uint256_t v1 = stack[-1]; stack[-1] = stack[-7]; stack[-7] = v1; break; }
        case SWAP7: { uint256_t v1 = stack[-1]; stack[-1] = stack[-8]; stack[-8] = v1; break; }
        case SWAP8: { uint256_t v1 = stack[-1]; stack[-1] = stack[-9]; stack[-9] = v1; break; }
        case SWAP9: { uint256_t v1 = stack[-1]; stack[-1] = stack[-10]; stack[-10] = v1; break; }
        case SWAP10: { uint256_t v1 = stack[-1]; stack[-1] = stack[-11]; stack[-11] = v1; break; }
        case SWAP11: { uint256_t v1 = stack[-1]; stack[-1] = stack[-12]; stack[-12] = v1; break; }
        case SWAP12: { uint256_t v1 = stack[-1]; stack[-1] = stack[-13]; stack[-13] = v1; break; }
        case SWAP13: { uint256_t v1 = stack[-1]; stack[-1] = stack[-14]; stack[-14] = v1; break; }
        case SWAP14: { uint256_t v1 = stack[-1]; stack[-1] = stack[-15]; stack[-15] = v1; break; }
        case SWAP15: { uint256_t v1 = stack[-1]; stack[-1] = stack[-16]; stack[-16] = v1; break; }
        case SWAP16: { uint256_t v1 = stack[-1]; stack[-1] = stack[-17]; stack[-17] = v1; break; }
        case LOG0: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            storage.log0(owner_address, buffer, size);
            break;
        }
        case LOG1: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            storage.log1(owner_address, v3, buffer, size);
            break;
        }
        case LOG2: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            storage.log2(owner_address, v3, v4, buffer, size);
            break;
        }
        case LOG3: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop(), v5 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            storage.log3(owner_address, v3, v4, v5, buffer, size);
            break;
        }
        case LOG4: {
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop(), v5 = stack.pop(), v6 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            uint8_t buffer[size];
            memory.dump(offset, size, buffer);
            storage.log4(owner_address, v3, v4, v5, v6, buffer, size);
            break;
        }
        case CREATE: {
            uint256_t value = stack.pop();
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t init_offset = v1.cast32(), init_size = v2.cast32();
            if (storage.balance(owner_address) < value) throw INSUFFICIENT_BALANCE;
            uint8_t init[init_size];
            memory.dump(init_offset, init_size, init);
            uint256_t code_address = (uint256_t)gen_address(owner_address, storage.nonce(owner_address));
            storage.increment_nonce(owner_address);
            // check conflict and throw
            uint32_t commit_id = storage.commit();
            // create account
            // optionally set nonce 1
            storage.sub_balance(owner_address, value);
            storage.add_balance(code_address, value);
            bool success;
            try {
                success = vm_run(release, block, storage,
                                origin_address, gas_price,
                                code_address, init, init_size,
                                owner_address, value, nullptr, 0,
                                return_data, return_size, return_capacity, gas,
                                false, depth+1);
            } catch (Error e) {
                success = false;
                return_size = 0;
            }
            if (success) storage.register_code(code_address, return_data, return_size);
            if (!success) storage.rollback(commit_id);
            stack.push(success);
            break;
        }
        case CALL: {
            uint256_t _gas = stack.pop();
            uint256_t code_address = stack.pop();
            uint256_t value = stack.pop();
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t args_offset = v1.cast32(), args_size = v2.cast32(), ret_offset = v3.cast32(), ret_size = v4.cast32();
            if (read_only && value != 0) throw ILLEGAL_UPDATE;
            if (storage.balance(owner_address) < value) throw INSUFFICIENT_BALANCE;
            uint8_t args_data[args_size];
            memory.dump(args_offset, args_size, args_data);
            const uint32_t code_size = storage.code_size(code_address);
            const uint8_t *code = storage.code(code_address);
            uint32_t commit_id = storage.commit();
            // create account or return
            storage.sub_balance(owner_address, value);
            storage.add_balance(code_address, value);
            bool success;
            try {
                success = vm_run(release, block, storage,
                                origin_address, gas_price,
                                code_address, code, code_size,
                                owner_address, value, args_data, args_size,
                                return_data, return_size, return_capacity, gas,
                                false, depth+1);
            } catch (Error e) {
                success = false;
                return_size = 0;
            }
            if (!success) storage.rollback(commit_id);
            uint32_t size = ret_size;
            if (size > return_size) size = return_size;
            memory.burn(ret_offset, size, return_data);
            memory.clear(ret_offset + size, ret_size - size);
            stack.push(success);
            break;
        }
        case CALLCODE: {
            uint256_t _gas = stack.pop();
            uint256_t code_address = stack.pop();
            uint256_t value = stack.pop();
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t args_offset = v1.cast32(), args_size = v2.cast32(), ret_offset = v3.cast32(), ret_size = v4.cast32();
            if (storage.balance(owner_address) < value) throw INSUFFICIENT_BALANCE;
            uint8_t args_data[args_size];
            memory.dump(args_offset, args_size, args_data);
            const uint32_t code_size = storage.code_size(code_address);
            const uint8_t *code = storage.code(code_address);
            uint32_t commit_id = storage.commit();
            bool success;
            try {
                success = vm_run(release, block, storage,
                                origin_address, gas_price,
                                owner_address, code, code_size,
                                caller_address, value, args_data, args_size,
                                return_data, return_size, return_capacity, gas,
                                false, depth+1);
            } catch (Error e) {
                success = false;
                return_size = 0;
            }
            if (!success) storage.rollback(commit_id);
            uint32_t size = ret_size;
            if (size > return_size) size = return_size;
            memory.burn(ret_offset, size, return_data);
            memory.clear(ret_offset + size, ret_size - size);
            stack.push(success);
            break;
        }
        case RETURN: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            return_size = size;
            _ensure_capacity(return_data, return_size, return_capacity);
            memory.dump(offset, return_size, return_data);
            return true;
        }
        case DELEGATECALL: {
            uint256_t _gas = stack.pop();
            uint256_t code_address = stack.pop();
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t args_offset = v1.cast32(), args_size = v2.cast32(), ret_offset = v3.cast32(), ret_size = v4.cast32();
            uint8_t args_data[args_size];
            memory.dump(args_offset, args_size, args_data);
            const uint32_t code_size = storage.code_size(code_address);
            const uint8_t *code = storage.code(code_address);
            uint32_t commit_id = storage.commit();
            bool success;
            try {
                success = vm_run(release, block, storage,
                                origin_address, gas_price,
                                owner_address, code, code_size,
                                caller_address, call_value, args_data, args_size,
                                return_data, return_size, return_capacity, gas,
                                false, depth+1);
            } catch (Error e) {
                success = false;
                return_size = 0;
            }
            if (!success) storage.rollback(commit_id);
            uint32_t size = ret_size;
            if (size > return_size) size = return_size;
            memory.burn(ret_offset, size, return_data);
            memory.clear(ret_offset + size, ret_size - size);
            stack.push(success);
            break;
        }
        case CREATE2: {
            uint256_t value = stack.pop();
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint256_t salt = stack.pop();
            uint32_t init_offset = v1.cast32(), init_size = v2.cast32();
            if (storage.balance(owner_address) < value) throw INSUFFICIENT_BALANCE;
            uint8_t init[init_size];
            memory.dump(init_offset, init_size, init);
            uint256_t code_address = (uint256_t)gen_address(owner_address, salt, sha3(init, init_size));
            storage.increment_nonce(owner_address);
            // check conflict and throw
            uint32_t commit_id = storage.commit();
            // create account
            // optionally set nonce 1
            storage.sub_balance(owner_address, value);
            storage.add_balance(code_address, value);
            bool success;
            try {
                success = vm_run(release, block, storage,
                                origin_address, gas_price,
                                code_address, init, init_size,
                                owner_address, value, nullptr, 0,
                                return_data, return_size, return_capacity, gas,
                                false, depth+1);
            } catch (Error e) {
                success = false;
                return_size = 0;
            }
            if (success) storage.register_code(code_address, return_data, return_size);
            if (!success) storage.rollback(commit_id);
            stack.push(success);
        }
        case STATICCALL: {
            uint256_t _gas = stack.pop();
            uint256_t code_address = stack.pop();
            uint256_t v1 = stack.pop(), v2 = stack.pop(), v3 = stack.pop(), v4 = stack.pop();
            uint32_t args_offset = v1.cast32(), args_size = v2.cast32(), ret_offset = v3.cast32(), ret_size = v4.cast32();
            storage.add_balance(code_address, 0);
            uint8_t args_data[args_size];
            memory.dump(args_offset, args_size, args_data);
            const uint32_t code_size = storage.code_size(code_address);
            const uint8_t *code = storage.code(code_address);
            uint32_t commit_id = storage.commit();
            bool success;
            try {
                success = vm_run(release, block, storage,
                                origin_address, gas_price,
                                code_address, code, code_size,
                                owner_address, 0, args_data, args_size,
                                return_data, return_size, return_capacity, gas,
                                true, depth+1);
            } catch (Error e) {
                success = false;
                return_size = 0;
            }
            if (!success) storage.rollback(commit_id);
            uint32_t size = ret_size;
            if (size > return_size) size = return_size;
            memory.burn(ret_offset, size, return_data);
            memory.clear(ret_offset + size, ret_size - size);
            stack.push(success);
            break;
        }
        case REVERT: {
            uint256_t v1 = stack.pop(), v2 = stack.pop();
            uint32_t offset = v1.cast32(), size = v2.cast32();
            return_size = size;
            _ensure_capacity(return_data, return_size, return_capacity);
            memory.dump(offset, return_size, return_data);
            return false;
        }
        case SELFDESTRUCT: {
            uint256_t v1 = stack.pop();
            uint256_t amount = storage.balance(owner_address);
            storage.add_balance(v1, amount);
            // kill contract
            return_size = 0;
            return true;
        }
        default: throw INVALID_OPCODE;
        }
    }
}

class _Storage : public Storage {
private:
    static constexpr int L = 1024;
    int account_size = 0;
    uint160_t account_index[L];
    struct account account_list[L];
    int keyvalue_size = 0;
    int keyvalue_index[L];
    uint256_t keyvalue_list[L][2];
    const struct account *find_account(const uint256_t &account) {
        for (int i = 0; i < account_size; i++) {
            if ((uint160_t)account == account_index[i]) return &account_list[i];
        }
        return nullptr;
    }
    void update(const uint256_t &account, const uint256_t &nonce, const uint256_t &balance) {
        int index = account_size;
        for (int i = 0; i < account_size; i++) {
            if ((uint160_t)account == account_index[i]) {
                index = i;
                break;
            }
        }
        if (index == account_size) {
            if (account_size == L) throw INSUFFICIENT_SPACE;
            account_index[account_size] = (uint160_t)account;
            account_list[account_size].nonce = 0;
            account_list[account_size].balance = 0;
            account_list[account_size].code = nullptr;
            account_list[account_size].code_size = 0;
            account_list[account_size].code_hash = 0;
            account_size++;
        }
        account_list[index].nonce = nonce;
        account_list[index].balance = balance;
    }
    void reset() {
        for (int i = 0; i < account_size; i++) delete account_list[i].code;
        account_size = 0;
        keyvalue_size = 0;
    }
    void load(uint32_t hash) {
        std::stringstream ss;
        ss << std::hex << std::setw(8) << std::setfill('0') << hash;
        std::string name(ss.str());
        std::ifstream fs("states/" + name + ".dat", std::ios::ate | std::ios::binary);
        if (!fs.is_open()) throw UNKNOWN_FILE;
        uint32_t size = fs.tellg();
        uint8_t buffer[size];
        fs.seekg(0, std::ios::beg);
        fs.read((char*)buffer, size);
        uint32_t offset = 0;
        account_size = b2w32le(&buffer[offset]); offset += 4;
        if (account_size > L) throw INSUFFICIENT_SPACE;
        for (int i = 0; i < account_size; i++) {
            account_index[i] = uint160_t::from(&buffer[offset]); offset += 20;
            account_list[i].nonce = uint256_t::from(&buffer[offset]); offset += 32;
            account_list[i].balance = uint256_t::from(&buffer[offset]); offset += 32;
            account_list[i].code_size = b2w32le(&buffer[offset]); offset += 4;
            account_list[i].code = new uint8_t[account_list[i].code_size];
            if (account_list[i].code == nullptr) throw MEMORY_EXAUSTED;
            for (uint32_t j = 0; j < account_list[i].code_size; j++) {
                account_list[i].code[j] = buffer[offset]; offset++;
            }
            account_list[i].code_hash = uint256_t::from(&buffer[offset]); offset += 32;
        }
        keyvalue_size = b2w32le(&buffer[offset]); offset += 4;
        if (keyvalue_size > L) throw INSUFFICIENT_SPACE;
        for (int i = 0; i < keyvalue_size; i++) {
            keyvalue_index[i] = b2w32le(&buffer[offset]); offset += 4;
            keyvalue_list[i][0] = uint256_t::from(&buffer[offset]); offset += 32;
            keyvalue_list[i][1] = uint256_t::from(&buffer[offset]); offset += 32;
        }
    }
    uint32_t dump() const {
        uint32_t size = 0;
        size += 4;
        for (int i = 0; i < account_size; i++) {
            size += 20 + 32 + 32 + 4 + account_list[i].code_size + 32;
        }
        size += 4;
        for (int i = 0; i < keyvalue_size; i++) {
            size += 4 + 32 + 32;
        }
        uint8_t buffer[size];
        uint32_t offset = 0;
        w2b32le(account_size, &buffer[offset]); offset += 4;
        for (int i = 0; i < account_size; i++) {
            uint160_t::to(account_index[i], &buffer[offset]); offset += 20;
            uint256_t::to(account_list[i].nonce, &buffer[offset]); offset += 32;
            uint256_t::to(account_list[i].balance, &buffer[offset]); offset += 32;
            w2b32le(account_list[i].code_size, &buffer[offset]); offset += 4;
            for (uint32_t j = 0; j < account_list[i].code_size; j++) {
                buffer[offset] = account_list[i].code[j]; offset++;
            }
            uint256_t::to(account_list[i].code_hash, &buffer[offset]); offset += 32;
        }
        w2b32le(keyvalue_size, &buffer[offset]); offset += 4;
        for (int i = 0; i < keyvalue_size; i++) {
            w2b32le(keyvalue_index[i], &buffer[offset]); offset += 4;
            uint256_t::to(keyvalue_list[i][0], &buffer[offset]); offset += 32;
            uint256_t::to(keyvalue_list[i][1], &buffer[offset]); offset += 32;
        }
        uint32_t hash = sha256(buffer, size).cast32();
        std::stringstream ss;
        ss << std::hex << std::setw(8) << std::setfill('0') << hash;
        std::string name(ss.str());
        std::ofstream fs("states/" + name + ".dat", std::ios::out | std::ios::binary);
        fs.write((const char*)buffer, size);
        return hash;
    }
public:
    _Storage() {
        const char *name = std::getenv("EVM_STATE");
        if (name != nullptr) {
            uint32_t hash;
            std::stringstream ss;
            ss << std::hex << name;
            ss >> hash;
            load(hash);
        }
    }
    ~_Storage() { reset(); }
    const uint256_t& load(const uint256_t &account, const uint256_t &address) {
        for (int i = 0; i < keyvalue_size; i++) {
            if (keyvalue_list[i][0] == address && (uint160_t)account == account_index[keyvalue_index[i]]) {
                return keyvalue_list[i][1];
            }
        }
        static uint256_t _0 = 0;
        return _0;
    }
    void store(const uint256_t &account, const uint256_t &address, const uint256_t& v) {
        for (int i = 0; i < keyvalue_size; i++) {
            if (keyvalue_list[i][0] == address && (uint160_t)account == account_index[keyvalue_index[i]]) {
                keyvalue_list[i][1] = v;
                return;
            }
        }
        int index = account_size;
        for (int i = 0; i < account_size; i++) {
            if ((uint160_t)account == account_index[i]) {
                index = i;
                break;
            }
        }
        if (index == account_size) {
            if (account_size == L) throw INSUFFICIENT_SPACE;
            account_list[account_size].nonce = 0;
            account_list[account_size].balance = 0;
            account_list[account_size].code = nullptr;
            account_list[account_size].code_size = 0;
            account_list[account_size].code_hash = 0;
            account_size++;
        }
        if (keyvalue_size == L) throw INSUFFICIENT_SPACE;
        keyvalue_index[keyvalue_size] = index;
        keyvalue_list[keyvalue_size][0] = address;
        keyvalue_list[keyvalue_size][1] = v;
        keyvalue_size++;
    }
    void register_code(const uint256_t &account, const uint8_t *buffer, uint32_t size) {
        int index = account_size;
        for (int i = 0; i < account_size; i++) {
            if ((uint160_t)account == account_index[i]) {
                index = i;
                break;
            }
        }
        if (index == account_size) {
            if (account_size == L) throw INSUFFICIENT_SPACE;
            account_index[account_size] = (uint160_t)account;
            account_list[account_size].nonce = 0;
            account_list[account_size].balance = 0;
            account_list[account_size].code = nullptr;
            account_list[account_size].code_size = 0;
            account_list[account_size].code_hash = 0;
            account_size++;
        }
        if (account_list[index].code != nullptr) throw CODE_CONFLICT;
        uint8_t *code = new uint8_t[size];
        if (code == nullptr) throw MEMORY_EXAUSTED;
        for (uint32_t i = 0; i < size; i++) code[i] = buffer[i];
        account_list[index].code = code;
        account_list[index].code_size = size;
        account_list[index].code_hash = sha3(code, size);
    }
    void log0(const uint256_t &owner, const uint8_t *buffer, uint32_t size) {}
    void log1(const uint256_t &owner, const uint256_t &v1, const uint8_t *buffer, uint32_t size) {}
    void log2(const uint256_t &owner, const uint256_t &v1, const uint256_t &v2, const uint8_t *buffer, uint32_t size) {}
    void log3(const uint256_t &owner, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint8_t *buffer, uint32_t size) {}
    void log4(const uint256_t &owner, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint256_t &v4, const uint8_t *buffer, uint32_t size) {}
    uint32_t commit() {
        uint32_t commit_id = dump();
        std::stringstream ss;
        ss << std::hex << std::setw(8) << std::setfill('0') << commit_id;
        std::string name(ss.str());
        std::cout << "EVM_STATE=" << name << std::endl;
        return commit_id;
    }
    void rollback(uint32_t commit_id) { reset(); load(commit_id); }
};

class _Block : public Block {
public:
    uint32_t chainid() {
        return 1; // configurable
    }
    uint32_t timestamp() {
        return 0; // eos block
    }
    uint32_t number() {
        return 0; // eos block
    }
    const uint256_t& coinbase() {
        static uint256_t t = 0; // static
        return t;
    }
    const uint256_t& gaslimit() {
        static uint256_t t = 10000000; // static
        return t;
    }
    const uint256_t& difficulty() {
        static uint256_t t = 0; // static
        return t;
    }
    uint256_t hash(uint32_t number) {
        uint8_t buffer[4];
        w2b32le(number, buffer);
        return (uint256_t)ripemd160(buffer, 4);
    }
};

static void raw(const uint8_t *buffer, uint32_t size, uint160_t sender)
{
    Release release = ISTANBUL;
    _Block block;
    _Storage storage;

    for (uint8_t i = ECRECOVER; i <= BLAKE2F; i++) storage.register_code(i, (uint8_t*)(intptr_t)i, 0);

    struct txn txn = decode_txn(buffer, size);
    if (!txn.is_signed) throw INVALID_TRANSACTION;

    uint32_t offset = 8 + 2 * block.chainid();
    if (txn.v != 27 && txn.v != 28 && txn.v != 27 + offset && txn.v != 28 + offset) throw INVALID_TRANSACTION;

    uint256_t h;
    {
        uint256_t v = txn.v;
        uint256_t r = txn.r;
        uint256_t s = txn.s;
        txn.is_signed = v > 28;
        txn.v = block.chainid();
        txn.r = 0;
        txn.s = 0;
        uint32_t unsigned_size = encode_txn(txn);
        uint8_t unsigned_buffer[unsigned_size];
        encode_txn(txn, unsigned_buffer, unsigned_size);
        h = sha3(unsigned_buffer, unsigned_size);
        txn.is_signed = true;
        txn.v = v > 28 ? v - offset : v;
        txn.r = r;
        txn.s = s;
    }

    uint256_t intrinsic_gas = _gas(release, txn.has_to ? GasTxMessageCall : GasTxContractCreation);
    uint32_t zero_count = 0;
    for (uint32_t i = 0; i < txn.data_size; i++) if (txn.data[i] == 0) zero_count++;
    intrinsic_gas += zero_count * _gas(release, GasTxDataZero);
    intrinsic_gas += (txn.data_size - zero_count) * _gas(release, GasTxDataNonZero);
    if (txn.gaslimit < intrinsic_gas) throw INVALID_TRANSACTION;
    uint256_t gas = txn.gaslimit - intrinsic_gas;

    uint256_t from = ecrecover(h, txn.v, txn.r, txn.s);
    uint256_t to = txn.to;
    if (!txn.has_to) to = (uint256_t)gen_address(from, storage.nonce(from));

    if (txn.nonce != storage.nonce(from)) throw NONCE_MISMATCH;
    storage.increment_nonce(from);

    if (storage.balance(from) < txn.gaslimit * txn.gasprice + txn.value) throw INSUFFICIENT_BALANCE;
    storage.sub_balance(from, txn.gaslimit * txn.gasprice);

    // NO TRANSACTION FAILURE FROM HERE
    uint32_t commit_id = storage.commit();

    storage.sub_balance(from, txn.value);
    storage.add_balance(to, txn.value);

    bool success = false;
    uint32_t return_size = 0;
    uint32_t return_capacity = 0;
    uint8_t *return_data = nullptr;

    // message call
    if (txn.has_to) {
        try {
            success = vm_run(release, block, storage,
                            from, txn.gasprice,
                            to, storage.code(to), storage.code_size(to),
                            from, txn.value, txn.data, txn.data_size,
                            return_data, return_size, return_capacity, gas,
                            false, 0);
        } catch (Error e) {
            success = false;
        }
    }

    // contract creation
    if (!txn.has_to) {
        try {
            success = vm_run(release, block, storage,
                            from, txn.gasprice,
                            to, txn.data, txn.data_size,
                            from, txn.value, nullptr, 0,
                            return_data, return_size, return_capacity, gas,
                            false, 0);
        } catch (Error e) {
            success = false;
        }
        if (success) storage.register_code(to, return_data, return_size);
    }

    delete return_data;
    if (!success) storage.rollback(commit_id);

    // TODO refund gas if failure with refund

    storage.add_balance(from, gas * txn.gasprice);
    // after execution delete self destruct accounts
    // after execution delete touched accounts that were zeroed
    storage.commit();
}

/* main */

static inline int hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return -1;
}

static inline bool parse_hex(const char *hexstr, uint8_t *buffer, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        int hi = hex(hexstr[2*i]);
        int lo = hex(hexstr[2*i+1]);
        if (hi < 0 || lo < 0) return false;
        buffer[i] = hi << 4 | lo;
    }
    return true;
}

int main(int argc, const char *argv[])
{
/*
    uint256_t v = 27;
    uint256_t h = uint256_t::from("\xda\xf5\xa7\x79\xae\x97\x2f\x97\x21\x97\x30\x3d\x7b\x57\x47\x46\xc7\xef\x83\xea\xda\xc0\xf2\x79\x1a\xd2\x3d\xb9\x2e\x4c\x8e\x53");
    uint256_t r = uint256_t::from("\x28\xef\x61\x34\x0b\xd9\x39\xbc\x21\x95\xfe\x53\x75\x67\x86\x60\x03\xe1\xa1\x5d\x3c\x71\xff\x63\xe1\x59\x06\x20\xaa\x63\x62\x76");
    uint256_t s = uint256_t::from("\x67\xcb\xe9\xd8\x99\x7f\x76\x1a\xec\xb7\x03\x30\x4b\x38\x00\xcc\xf5\x55\xc9\xf3\xdc\x64\x21\x4b\x29\x7f\xb1\x96\x6a\x3b\x6d\x83");
    std::cerr << v << std::endl;
    std::cerr << h << std::endl;
    std::cerr << r << std::endl;
    std::cerr << s << std::endl;
    uint256_t a = ecrecover(h, v, r, s);
    std::cerr << a << std::endl;
// 000000000000000000000000000000000000000000000000000000000000001b
// daf5a779ae972f972197303d7b574746c7ef83eadac0f2791ad23db92e4c8e53
// 28ef61340bd939bc2195fe537567866003e1a15d3c71ff63e1590620aa636276
// 67cbe9d8997f761aecb703304b3800ccf555c9f3dc64214b297fb1966a3b6d83
// 0000000000000000000000009d8a62f656a8d1615c1294fd71e9cfb3e4855a4f
*/
    const char *progname = argv[0];
    if (argc < 2) { std::cerr << "usage: " << progname << " <hex>" << std::endl; return 1; }
    const char *hexstr = argv[1];
    int len = std::strlen(hexstr);
    uint32_t size = len / 2;
    uint8_t buffer[size];
    if (len % 2 > 0 || !parse_hex(hexstr, buffer, size)) { std::cerr << progname << ": invalid input" << std::endl; return 1; }
    try {
        raw(buffer, size, 0);
    } catch (Error e) {
        std::cerr << progname << ": error " << errors[e] << std::endl; return 1;
    }
    return 0;
}
