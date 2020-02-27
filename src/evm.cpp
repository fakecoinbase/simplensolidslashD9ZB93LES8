#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdint.h>

#include "evm.hpp"

// a simple state implementation for testing
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
        local<uint8_t> buffer_l(size); uint8_t *buffer = buffer_l.data;
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
        local<uint8_t> buffer_l(size); uint8_t *buffer = buffer_l.data;
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
//        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: get_nonce " << address << std::endl;
        const struct account *account = find(address);
        return account == nullptr ? 0 : account->nonce;
    };
    inline void set_nonce(const uint160_t &address, const uint64_t &nonce) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: set_nonce " << address << " " << (uint256_t)nonce << std::endl;
        update(address, nonce, get_balance(address), get_codehash(address));
    };
    inline uint256_t get_balance(const uint160_t &address) const {
//        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: get_balance " << address << std::endl;
        const struct account *account = find(address);
        return account == nullptr ? 0 : account->balance;
    };
    inline void set_balance(const uint160_t &address, const uint256_t &balance) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: set_balance " << address << " " << balance << std::endl;
        update(address, get_nonce(address), balance, get_codehash(address));
    };
    inline uint256_t get_codehash(const uint160_t &address) const {
//        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: get_codehash " << address << std::endl;
        const struct account *account = find(address);
        return account == nullptr ? 0 : account->codehash;
    };
    inline void set_codehash(const uint160_t &address, const uint256_t &codehash) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: set_codehash " << address << " " << codehash << std::endl;
        update(address, get_nonce(address), get_balance(address), codehash);
    };

    inline uint8_t *load_code(const uint256_t &codehash, uint64_t &code_size) const {
//        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: load_code " << codehash << std::endl;
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
//        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: load " << address << " " << key << std::endl;
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
        for (int i = 0; i < account_size; i++) {
            if (address == account_index[i]) {
                account_size--;
                if (i < account_size) {
                    account_index[i] = account_index[account_size];
                    account_list[i].nonce = account_list[account_size].nonce;
                    account_list[i].balance = account_list[account_size].balance;
                    account_list[i].codehash = account_list[account_size].codehash;
                }
                return;
            }
        }
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

// a simple block implementation for testing
class _Block : public Block {
private:
    uint64_t _number = 10000000;
    uint64_t _timestamp = 0;
    uint64_t _gaslimit = 10000000;
    uint160_t _coinbase = 0;
    uint256_t _difficulty = 17179869184;
    static uint64_t now() { return std::time(nullptr); }
public:
    _Block() : _timestamp(now()) {}
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

// this marker is used by the tester to reuse the code above
// ** main **

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
    const char *progname = argv[0];
    if (argc < 2) { std::cerr << "usage: " << progname << " <hex>" << std::endl; return 1; }
    const char *hexstr = argv[1];
    int len = std::strlen(hexstr);
    uint64_t size = len / 2;
    local<uint8_t> buffer_l(size); uint8_t *buffer = buffer_l.data;
    if (len % 2 > 0 || !parse_hex(hexstr, buffer, size)) { std::cerr << progname << ": invalid input" << std::endl; return 1; }
    _try({
        _Block block;
        _State state;
        _catches(vm_txn)(block, state, buffer, size, 65535, false);
        state.save();
    }, Error e, {
        std::cerr << progname << ": error " << errors[e] << std::endl; return 1;
    })
    return 0;
}
