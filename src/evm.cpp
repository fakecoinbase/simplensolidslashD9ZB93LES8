#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdint.h>

#include "evm.hpp"

class _State : public State {
private:
    struct account {
        uint64_t nonce;
        uint256_t balance;
        uint256_t codehash;
    };
    struct contract {
        uint8_t *code;
        uint64_t code_size;
    };
    static constexpr int L = 1024;
    int account_size = 0;
    uint160_t account_index[L];
    struct account account_list[L];
    int keyvalue_size = 0;
    int keyvalue_index[L];
    uint256_t keyvalue_list[L][2];
    int contract_size = 0;
    uint256_t contract_index[L];
    struct contract contract_list[L];
    const struct account *find(const uint160_t &account) const {
        for (int i = 0; i < account_size; i++) {
            if (account == account_index[i]) return &account_list[i];
        }
        return nullptr;
    }
    const struct contract *find(const uint256_t &codehash) const {
        for (int i = 0; i < contract_size; i++) {
            if (codehash == contract_index[i]) return &contract_list[i];
        }
        return nullptr;
    }
    void reset() {
        for (int i = 0; i < contract_size; i++) _delete(contract_list[i].code);
        account_size = 0;
        keyvalue_size = 0;
        contract_size = 0;
    }
    void update(const uint160_t &account, const uint64_t &nonce, const uint256_t &balance, const uint256_t &codehash) {
        int index = account_size;
        for (int i = 0; i < account_size; i++) {
            if (account == account_index[i]) {
                index = i;
                break;
            }
        }
        if (index == account_size) {
            if (account_size == L) throw INSUFFICIENT_SPACE;
            account_index[account_size] = account;
            account_list[account_size].nonce = 0;
            account_list[account_size].balance = 0;
            account_list[account_size].codehash = 0;
            account_size++;
        }
        account_list[index].nonce = nonce;
        account_list[index].balance = balance;
        account_list[index].codehash = codehash;
    }
    void update(const uint256_t &codehash, const uint8_t *buffer, uint64_t size) {
        int index = contract_size;
        for (int i = 0; i < contract_size; i++) {
            if (codehash == contract_index[i]) {
                index = i;
                break;
            }
        }
        if (index == contract_size) {
            if (contract_size == L) throw INSUFFICIENT_SPACE;
            contract_index[contract_size] = codehash;
            contract_list[contract_size].code = nullptr;
            contract_list[contract_size].code_size = 0;
            contract_size++;
        }
        uint8_t *code = _new<uint8_t>(size);
        for (uint64_t i = 0; i < size; i++) code[i] = buffer[i];
        contract_list[index].code = code;
        contract_list[index].code_size = size;
    }
    void load(uint64_t hash) {
        std::stringstream ss;
        ss << std::hex << std::setw(8) << std::setfill('0') << hash;
        std::string name(ss.str());
        std::ifstream fs("states/" + name + ".dat", std::ios::ate | std::ios::binary);
        if (!fs.is_open()) assert(false);
        uint64_t size = fs.tellg();
        uint8_t buffer[size];
        fs.seekg(0, std::ios::beg);
        fs.read((char*)buffer, size);
        uint64_t offset = 0;
        account_size = b2w32le(&buffer[offset]); offset += 4;
        if (account_size > L) throw INSUFFICIENT_SPACE;
        for (int i = 0; i < account_size; i++) {
            account_index[i] = uint160_t::from(&buffer[offset]); offset += 20;
            account_list[i].nonce = uint256_t::from(&buffer[offset]).cast64(); offset += 32;
            account_list[i].balance = uint256_t::from(&buffer[offset]); offset += 32;
            uint64_t code_size = b2w32le(&buffer[offset]); offset += 4;
            uint64_t code_offset = offset; offset += code_size;
            account_list[i].codehash = uint256_t::from(&buffer[offset]); offset += 32;
            update(account_list[i].codehash, &buffer[code_offset], code_size);
        }
        keyvalue_size = b2w32le(&buffer[offset]); offset += 4;
        if (keyvalue_size > L) throw INSUFFICIENT_SPACE;
        for (int i = 0; i < keyvalue_size; i++) {
            keyvalue_index[i] = b2w32le(&buffer[offset]); offset += 4;
            keyvalue_list[i][0] = uint256_t::from(&buffer[offset]); offset += 32;
            keyvalue_list[i][1] = uint256_t::from(&buffer[offset]); offset += 32;
        }
    }
    uint64_t dump() const {
        uint64_t size = 0;
        size += 4;
        for (int i = 0; i < account_size; i++) {
            uint64_t code_size;
            const uint8_t *code = load_code(account_list[i].codehash, code_size);
            _delete(code);
            size += 20 + 32 + 32 + 4 + code_size + 32;
        }
        size += 4;
        for (int i = 0; i < keyvalue_size; i++) {
            size += 4 + 32 + 32;
        }
        uint8_t buffer[size];
        uint64_t offset = 0;
        w2b32le(account_size, &buffer[offset]); offset += 4;
        for (int i = 0; i < account_size; i++) {
            uint64_t code_size;
            const uint8_t *code = load_code(account_list[i].codehash, code_size);
            uint160_t::to(account_index[i], &buffer[offset]); offset += 20;
            uint256_t::to(account_list[i].nonce, &buffer[offset]); offset += 32;
            uint256_t::to(account_list[i].balance, &buffer[offset]); offset += 32;
            w2b32le(code_size, &buffer[offset]); offset += 4;
            for (uint64_t j = 0; j < code_size; j++) {
                buffer[offset] = code[j]; offset++;
            }
            uint256_t::to(account_list[i].codehash, &buffer[offset]); offset += 32;
            _delete(code);
        }
        w2b32le(keyvalue_size, &buffer[offset]); offset += 4;
        for (int i = 0; i < keyvalue_size; i++) {
            w2b32le(keyvalue_index[i], &buffer[offset]); offset += 4;
            uint256_t::to(keyvalue_list[i][0], &buffer[offset]); offset += 32;
            uint256_t::to(keyvalue_list[i][1], &buffer[offset]); offset += 32;
        }
        uint256_t sha = sha256(buffer, size);
        uint64_t hash = 0
            | (uint64_t)sha.byte(7) << 56
            | (uint64_t)sha.byte(6) << 48
            | (uint64_t)sha.byte(5) << 40
            | (uint64_t)sha.byte(4) << 32
            | (uint64_t)sha.byte(3) << 24
            | (uint64_t)sha.byte(2) << 16
            | (uint64_t)sha.byte(1) << 8
            | (uint64_t)sha.byte(0);
        std::stringstream ss;
        ss << std::hex << std::setw(8) << std::setfill('0') << hash;
        std::string name(ss.str());
        std::ofstream fs("states/" + name + ".dat", std::ios::out | std::ios::binary);
        fs.write((const char*)buffer, size);
        return hash;
    }
public:
    _State() {
        const char *name = std::getenv("EVM_STATE");
        if (name != nullptr) {
            uint64_t hash;
            std::stringstream ss;
            ss << std::hex << name;
            ss >> hash;
            load(hash);
        }
    }
    ~_State() { reset(); }
    void save() {
        uint64_t hash = dump();
        std::stringstream ss;
        ss << std::hex << std::setw(8) << std::setfill('0') << hash;
        std::string name(ss.str());
        std::cout << "EVM_STATE=" << name << std::endl;
    }
    inline uint64_t get_nonce(const uint160_t &address) const {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: get_nonce " << address << std::endl;
        const struct account *account = find(address);
        return account == nullptr ? 0 : account->nonce;
    };
    inline void set_nonce(const uint160_t &address, const uint64_t &nonce) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: set_nonce " << address << " " << (uint256_t)nonce << std::endl;
        update(address, nonce, get_balance(address), get_codehash(address));
    };
    inline uint256_t get_balance(const uint160_t &address) const {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: get_balance " << address << std::endl;
        const struct account *account = find(address);
        return account == nullptr ? 0 : account->balance;
    };
    inline void set_balance(const uint160_t &address, const uint256_t &balance) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: set_balance " << address << " " << balance << std::endl;
        update(address, get_nonce(address), balance, get_codehash(address));
    };
    inline uint256_t get_codehash(const uint160_t &address) const {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: get_codehash " << address << std::endl;
        const struct account *account = find(address);
        return account == nullptr ? 0 : account->codehash;
    };
    inline void set_codehash(const uint160_t &address, const uint256_t &codehash) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: set_codehash " << address << " " << codehash << std::endl;
        update(address, get_nonce(address), get_balance(address), codehash);
    };

    inline uint8_t *load_code(const uint256_t &codehash, uint64_t &code_size) const {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: load_code " << codehash << std::endl;
        const struct contract *contract = find(codehash);
        if (contract == nullptr) {
            code_size = 0;
            return nullptr;
        }
        code_size = contract->code_size;
        uint8_t *code = _new<uint8_t>(code_size);
        for (uint64_t i = 0; i < code_size; i++) code[i] = contract->code[i];
        return code;
    };
    inline void store_code(const uint256_t &codehash, const uint8_t *code, uint64_t code_size) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: store_code " << codehash << std::endl;
        update(codehash, code, code_size);
    };

    inline uint256_t load(const uint160_t &address, const uint256_t &key) const {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: load " << address << " " << key << std::endl;
        for (int i = 0; i < keyvalue_size; i++) {
            if (keyvalue_list[i][0] == key && address == account_index[keyvalue_index[i]]) {
                return keyvalue_list[i][1];
            }
        }
        static uint256_t _0 = 0;
        return _0;
    };
    inline void store(const uint160_t &address, const uint256_t &key, const uint256_t& value) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: store " << address << " " << key << " " << value << std::endl;
        for (int i = 0; i < keyvalue_size; i++) {
            if (keyvalue_list[i][0] == key && address == account_index[keyvalue_index[i]]) {
                keyvalue_list[i][1] = value;
                return;
            }
        }
        int index = account_size;
        for (int i = 0; i < account_size; i++) {
            if (address == account_index[i]) {
                index = i;
                break;
            }
        }
        if (index == account_size) {
            if (account_size == L) throw INSUFFICIENT_SPACE;
            account_list[account_size].nonce = 0;
            account_list[account_size].balance = 0;
            account_list[account_size].codehash = 0;
            account_size++;
        }
        if (keyvalue_size == L) throw INSUFFICIENT_SPACE;
        keyvalue_index[keyvalue_size] = index;
        keyvalue_list[keyvalue_size][0] = key;
        keyvalue_list[keyvalue_size][1] = value;
        keyvalue_size++;
    };
    inline void remove(const uint160_t &address) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: remove " << address << std::endl;
        // TODO missing removal
    }
    inline void log0(const uint160_t &address, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log0 " << address << std::endl;
    }
    inline void log1(const uint160_t &address, const uint256_t &v1, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log1 " << address << " " << v1 << std::endl;
    }
    inline void log2(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log2 " << address << " " << v1 << " " << v2 << std::endl;
    }
    inline void log3(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log3 " << address << " " << v1 << " " << v2 << " " << v3 << std::endl;
    }
    inline void log4(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint256_t &v4, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log4 " << address << " " << v1 << " " << v2 << " " << v3 << " " << v4 << std::endl;
    }
};

class _Block : public Block {
private:
    uint64_t _number = 10000000;
    uint64_t _timestamp = 0;
    uint64_t _gaslimit = 10000000;
    uint160_t _coinbase = 0;
    uint256_t _difficulty = 17179869184;
public:
    _Block() {}
    _Block(uint64_t number, uint64_t timestamp, uint64_t gaslimit, const uint160_t& coinbase, const uint256_t& difficulty)
        : _number(number), _timestamp(timestamp), _gaslimit(gaslimit), _coinbase(coinbase), _difficulty(difficulty) {}
    uint64_t forknumber() { return _number; }
    uint64_t number() { return _number; }
    uint64_t timestamp() { return _timestamp; }
    uint64_t gaslimit() { return _gaslimit; }
    uint160_t coinbase() { return _coinbase; }
    uint256_t difficulty() { return _difficulty; }
    uint256_t hash(const uint256_t &number) {
        uint8_t buffer[32];
        uint256_t::to(number, buffer);
        return sha3(buffer, 32);
    }
};

/* main */

static inline int hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return -1;
}

static inline bool parse_hex(const char *hexstr, uint8_t *buffer, uint64_t size)
{
    for (uint64_t i = 0; i < size; i++) {
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
    if (true) {
        try {
            uint8_t in[3*32+3*512];
            uint8_t out[512];
            for (int i = 0; i < 3*32+3*512; i++) in[i] = 0;
            in[30] = 2;
            in[62] = 2;
            in[94] = 2;
            for (int i = 3*32+0*512+200; i < 3*32+1*512; i++) in[i] = (uint8_t)3*i;
            for (int i = 3*32+1*512+310; i < 3*32+2*512; i++) in[i] = (uint8_t)2*i;
            for (int i = 3*32+2*512+100; i < 3*32+3*512; i++) in[i] = (uint8_t)1*i;
            for (int i = 0; i < 3*32+3*512; i++) {
                std::cerr << (int)in[i] << " ";
            }
            std::cerr << std::endl;
            test(3*32+3*512, in, 512, 512, out);
            for (int i = 0; i < 512; i++) {
                std::cerr << (int)out[i] << " ";
            }
            std::cerr << std::endl;
        } catch (Error e) {
            std::cerr << "error " << errors[e] << std::endl; return 1;
        }
        return 0;
    }
*/
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
    uint64_t size = len / 2;
    uint8_t buffer[size];
    if (len % 2 > 0 || !parse_hex(hexstr, buffer, size)) { std::cerr << progname << ": invalid input" << std::endl; return 1; }
    _try({
        _Block block;
        _State state;
        state.set_balance(1, 10000000);
        _catches(vm_txn)(block, state, buffer, size, 1, true);
        state.save();
    }, Error e, {
        std::cerr << progname << ": error " << errors[e] << std::endl; return 1;
    })
    return 0;
}
