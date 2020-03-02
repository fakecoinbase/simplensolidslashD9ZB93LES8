#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdint.h>

#include "evm.hpp"

// just remove spaces from string
static inline std::string trim(std::string& s)
{
    s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
    s.erase(s.find_last_not_of(" \t\n\r\f\v")+1);
    return s;
}

// executes command and get the hash provided
static uint256_t exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) abort();
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) result += buffer.data();
    result = trim(result);
    return uhex256(result.c_str());
}

// handy function to convert hex digit into char
static char hexc(uint8_t b)
{
    assert(b < 16);
    return b < 10 ? (char)b + '0' : (char)(b - 10) + 'a';
}

// handy fnction to conver U<N> into hex chars
template<int N>
static void to_hex(const U<N>& v, char *chars)
{
    local<uint8_t> buffer_l(N/8); uint8_t *buffer = buffer_l.data;
    U<N>::to(v, buffer);
    for (uint64_t i = 0; i < N/8; i++) {
        chars[2*i] = hexc(buffer[i] >> 4);
        chars[2*i+1] = hexc(buffer[i] & 0xf);
    }
}

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
    static constexpr int L = 4096;
    uint64_t account_size = 0;
    uint160_t account_index[L];
    struct account account_list[L];
    uint64_t keyvalue_size = 0;
    uint64_t keyvalue_index[L];
    uint256_t keyvalue_list[L][2];
    uint64_t contract_size = 0;
    uint256_t contract_index[L];
    struct contract contract_list[L];
    struct rlp logs = { true, 0, nullptr };
    const struct account *find(const uint160_t &account) const {
        for (uint64_t i = 0; i < account_size; i++) {
            if (account == account_index[i]) return &account_list[i];
        }
        return nullptr;
    }
    const struct contract *find(const uint256_t &codehash) const {
        for (uint64_t i = 0; i < contract_size; i++) {
            if (codehash == contract_index[i]) return &contract_list[i];
        }
        return nullptr;
    }
    void reset() {
        for (uint64_t i = 0; i < contract_size; i++) _delete(contract_list[i].code);
        account_size = 0;
        keyvalue_size = 0;
        contract_size = 0;
    }
    void update(const uint160_t &account, const uint64_t &nonce, const uint256_t &balance, const uint256_t &codehash) {
        uint64_t index = account_size;
        for (uint64_t i = 0; i < account_size; i++) {
            if (account == account_index[i]) {
                index = i;
                break;
            }
        }
        if (index == account_size) {
            assert(account_size < L);
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
        uint64_t index = contract_size;
        for (uint64_t i = 0; i < contract_size; i++) {
            if (codehash == contract_index[i]) {
                index = i;
                break;
            }
        }
        if (index == contract_size) {
            assert(contract_size < L);
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
        assert(account_size <= L);
        for (uint64_t i = 0; i < account_size; i++) {
            account_index[i] = uint160_t::from(&buffer[offset]); offset += 20;
            account_list[i].nonce = uint256_t::from(&buffer[offset]).cast64(); offset += 32;
            account_list[i].balance = uint256_t::from(&buffer[offset]); offset += 32;
            uint64_t code_size = b2w32le(&buffer[offset]); offset += 4;
            uint64_t code_offset = offset; offset += code_size;
            account_list[i].codehash = uint256_t::from(&buffer[offset]); offset += 32;
            update(account_list[i].codehash, &buffer[code_offset], code_size);
        }
        keyvalue_size = b2w32le(&buffer[offset]); offset += 4;
        assert(keyvalue_size <= L);
        for (uint64_t i = 0; i < keyvalue_size; i++) {
            keyvalue_index[i] = b2w32le(&buffer[offset]); offset += 4;
            keyvalue_list[i][0] = uint256_t::from(&buffer[offset]); offset += 32;
            keyvalue_list[i][1] = uint256_t::from(&buffer[offset]); offset += 32;
        }
    }
    uint64_t dump() const {
        uint64_t size = 0;
        size += 4;
        for (uint64_t i = 0; i < account_size; i++) {
            uint64_t code_size;
            const uint8_t *code = load_code(account_list[i].codehash, code_size);
            _delete(code);
            size += 20 + 32 + 32 + 4 + code_size + 32;
        }
        size += 4;
        for (uint64_t i = 0; i < keyvalue_size; i++) {
            size += 4 + 32 + 32;
        }
        local<uint8_t> buffer_l(size); uint8_t *buffer = buffer_l.data;
        uint64_t offset = 0;
        w2b32le(account_size, &buffer[offset]); offset += 4;
        for (uint64_t i = 0; i < account_size; i++) {
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
        for (uint64_t i = 0; i < keyvalue_size; i++) {
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
    ~_State() { reset(); free_rlp(logs); }
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
        for (uint64_t i = 0; i < keyvalue_size; i++) {
            if (keyvalue_list[i][0] == key && address == account_index[keyvalue_index[i]]) {
                return keyvalue_list[i][1];
            }
        }
        static uint256_t _0 = 0;
        return _0;
    };
    inline void store(const uint160_t &address, const uint256_t &key, const uint256_t& value) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: store " << address << " " << key << " " << value << std::endl;
        for (uint64_t i = 0; i < keyvalue_size; i++) {
            if (keyvalue_list[i][0] == key && address == account_index[keyvalue_index[i]]) {
                keyvalue_list[i][1] = value;
                return;
            }
        }
        uint64_t index = account_size;
        for (uint64_t i = 0; i < account_size; i++) {
            if (address == account_index[i]) {
                index = i;
                break;
            }
        }
        if (index == account_size) {
            assert(account_size < L);
            account_list[account_size].nonce = 0;
            account_list[account_size].balance = 0;
            account_list[account_size].codehash = 0;
            account_size++;
        }
        assert (keyvalue_size < L);
        keyvalue_index[keyvalue_size] = index;
        keyvalue_list[keyvalue_size][0] = key;
        keyvalue_list[keyvalue_size][1] = value;
        keyvalue_size++;
    };
    inline void clear(const uint160_t &address) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: clear " << address << std::endl;
        for (uint64_t i = 0; i < keyvalue_size; i++) {
            if (address == account_index[keyvalue_index[i]]) {
                keyvalue_size--;
                if (i < keyvalue_size) {
                    keyvalue_index[i] = keyvalue_index[keyvalue_size];
                    keyvalue_list[i][0] = keyvalue_list[keyvalue_size][0];
                    keyvalue_list[i][1] = keyvalue_list[keyvalue_size][1];
                }
            }
        }
    }
    inline void remove(const uint160_t &address) {
        if (std::getenv("EVM_DEBUG"))  std::cout << "debug: remove " << address << std::endl;
        for (uint64_t i = 0; i < account_size; i++) {
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
        struct rlp *new_list = _new<struct rlp>(logs.size + 1);
        for (uint64_t i = 0; i < logs.size; i++) new_list[i] = logs.list[i];
        _delete(logs.list);
        logs.list = new_list;
        logs.size++;
        struct rlp *rlp = &logs.list[logs.size-1];
        rlp->is_list = true;
        rlp->size = 3;
        rlp->list = _new<struct rlp>(rlp->size);
        rlp->list[0].is_list = false;
        rlp->list[0].size = 20;
        rlp->list[0].data = _new<uint8_t>(rlp->list[0].size);
        uint160_t::to(address, rlp->list[0].data);
        rlp->list[1].is_list = true;
        rlp->list[1].size = 0;
        rlp->list[1].list = nullptr;
        rlp->list[2].is_list = false;
        rlp->list[2].size = data_size;
        rlp->list[2].data = _new<uint8_t>(rlp->list[2].size);
        for (uint64_t i = 0; i < data_size; i++) rlp->list[2].data[i] = data[i];
    }
    inline void log1(const uint160_t &address, const uint256_t &v1, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log1 " << address << " " << v1 << std::endl;
        struct rlp *new_list = _new<struct rlp>(logs.size + 1);
        for (uint64_t i = 0; i < logs.size; i++) new_list[i] = logs.list[i];
        _delete(logs.list);
        logs.list = new_list;
        logs.size++;
        struct rlp *rlp = &logs.list[logs.size-1];
        rlp->is_list = true;
        rlp->size = 3;
        rlp->list = _new<struct rlp>(rlp->size);
        rlp->list[0].is_list = false;
        rlp->list[0].size = 20;
        rlp->list[0].data = _new<uint8_t>(rlp->list[0].size);
        uint160_t::to(address, rlp->list[0].data);
        rlp->list[1].is_list = true;
        rlp->list[1].size = 1;
        rlp->list[1].list = _new<struct rlp>(rlp->list[1].size);
        rlp->list[1].list[0].is_list = false;
        rlp->list[1].list[0].size = 32;
        rlp->list[1].list[0].data = _new<uint8_t>(rlp->list[1].list[0].size);
        uint256_t::to(v1, rlp->list[1].list[0].data);
        rlp->list[2].is_list = false;
        rlp->list[2].size = data_size;
        rlp->list[2].data = _new<uint8_t>(rlp->list[2].size);
        for (uint64_t i = 0; i < data_size; i++) rlp->list[2].data[i] = data[i];
    }
    inline void log2(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log2 " << address << " " << v1 << " " << v2 << std::endl;
        struct rlp *new_list = _new<struct rlp>(logs.size + 1);
        for (uint64_t i = 0; i < logs.size; i++) new_list[i] = logs.list[i];
        _delete(logs.list);
        logs.list = new_list;
        logs.size++;
        struct rlp *rlp = &logs.list[logs.size-1];
        rlp->is_list = true;
        rlp->size = 3;
        rlp->list = _new<struct rlp>(rlp->size);
        rlp->list[0].is_list = false;
        rlp->list[0].size = 20;
        rlp->list[0].data = _new<uint8_t>(rlp->list[0].size);
        uint160_t::to(address, rlp->list[0].data);
        rlp->list[1].is_list = true;
        rlp->list[1].size = 2;
        rlp->list[1].list = _new<struct rlp>(rlp->list[1].size);
        rlp->list[1].list[0].is_list = false;
        rlp->list[1].list[0].size = 32;
        rlp->list[1].list[0].data = _new<uint8_t>(rlp->list[1].list[0].size);
        uint256_t::to(v1, rlp->list[1].list[0].data);
        rlp->list[1].list[1].is_list = false;
        rlp->list[1].list[1].size = 32;
        rlp->list[1].list[1].data = _new<uint8_t>(rlp->list[1].list[1].size);
        uint256_t::to(v2, rlp->list[1].list[1].data);
        rlp->list[2].is_list = false;
        rlp->list[2].size = data_size;
        rlp->list[2].data = _new<uint8_t>(rlp->list[2].size);
        for (uint64_t i = 0; i < data_size; i++) rlp->list[2].data[i] = data[i];
    }
    inline void log3(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log3 " << address << " " << v1 << " " << v2 << " " << v3 << std::endl;
        struct rlp *new_list = _new<struct rlp>(logs.size + 1);
        for (uint64_t i = 0; i < logs.size; i++) new_list[i] = logs.list[i];
        _delete(logs.list);
        logs.list = new_list;
        logs.size++;
        struct rlp *rlp = &logs.list[logs.size-1];
        rlp->is_list = true;
        rlp->size = 3;
        rlp->list = _new<struct rlp>(rlp->size);
        rlp->list[0].is_list = false;
        rlp->list[0].size = 20;
        rlp->list[0].data = _new<uint8_t>(rlp->list[0].size);
        uint160_t::to(address, rlp->list[0].data);
        rlp->list[1].is_list = true;
        rlp->list[1].size = 3;
        rlp->list[1].list = _new<struct rlp>(rlp->list[1].size);
        rlp->list[1].list[0].is_list = false;
        rlp->list[1].list[0].size = 32;
        rlp->list[1].list[0].data = _new<uint8_t>(rlp->list[1].list[0].size);
        uint256_t::to(v1, rlp->list[1].list[0].data);
        rlp->list[1].list[1].is_list = false;
        rlp->list[1].list[1].size = 32;
        rlp->list[1].list[1].data = _new<uint8_t>(rlp->list[1].list[1].size);
        uint256_t::to(v2, rlp->list[1].list[1].data);
        rlp->list[1].list[2].is_list = false;
        rlp->list[1].list[2].size = 32;
        rlp->list[1].list[2].data = _new<uint8_t>(rlp->list[1].list[2].size);
        uint256_t::to(v3, rlp->list[1].list[2].data);
        rlp->list[2].is_list = false;
        rlp->list[2].size = data_size;
        rlp->list[2].data = _new<uint8_t>(rlp->list[2].size);
        for (uint64_t i = 0; i < data_size; i++) rlp->list[2].data[i] = data[i];
    }
    inline void log4(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint256_t &v4, const uint8_t *data, uint64_t data_size) {
        if (std::getenv("EVM_DEBUG")) std::cout << "log4 " << address << " " << v1 << " " << v2 << " " << v3 << " " << v4 << std::endl;
        struct rlp *new_list = _new<struct rlp>(logs.size + 1);
        for (uint64_t i = 0; i < logs.size; i++) new_list[i] = logs.list[i];
        _delete(logs.list);
        logs.list = new_list;
        logs.size++;
        struct rlp *rlp = &logs.list[logs.size-1];
        rlp->is_list = true;
        rlp->size = 3;
        rlp->list = _new<struct rlp>(rlp->size);
        rlp->list[0].is_list = false;
        rlp->list[0].size = 20;
        rlp->list[0].data = _new<uint8_t>(rlp->list[0].size);
        uint160_t::to(address, rlp->list[0].data);
        rlp->list[1].is_list = true;
        rlp->list[1].size = 4;
        rlp->list[1].list = _new<struct rlp>(rlp->list[1].size);
        rlp->list[1].list[0].is_list = false;
        rlp->list[1].list[0].size = 32;
        rlp->list[1].list[0].data = _new<uint8_t>(rlp->list[1].list[0].size);
        uint256_t::to(v1, rlp->list[1].list[0].data);
        rlp->list[1].list[1].is_list = false;
        rlp->list[1].list[1].size = 32;
        rlp->list[1].list[1].data = _new<uint8_t>(rlp->list[1].list[1].size);
        uint256_t::to(v2, rlp->list[1].list[1].data);
        rlp->list[1].list[2].is_list = false;
        rlp->list[1].list[2].size = 32;
        rlp->list[1].list[2].data = _new<uint8_t>(rlp->list[1].list[2].size);
        uint256_t::to(v3, rlp->list[1].list[2].data);
        rlp->list[1].list[3].is_list = false;
        rlp->list[1].list[3].size = 32;
        rlp->list[1].list[3].data = _new<uint8_t>(rlp->list[1].list[3].size);
        uint256_t::to(v4, rlp->list[1].list[3].data);
        rlp->list[2].is_list = false;
        rlp->list[2].size = data_size;
        rlp->list[2].data = _new<uint8_t>(rlp->list[2].size);
        for (uint64_t i = 0; i < data_size; i++) rlp->list[2].data[i] = data[i];
    }
    uint256_t loghash() {
        _try({
            uint64_t size = _catches(dump_rlp)(logs, nullptr, 0);
            local<uint8_t> buffer_l(size); uint8_t *buffer = buffer_l.data;
            _catches(dump_rlp)(logs, buffer, size);
            return sha3(buffer, size);
        }, Error e, {
            assert(false);
        })
        return 0;
    }
    uint256_t root(const char *cmd) {
        uint64_t size = strlen(cmd) + 1 + 1 + account_size * ( 40 + 3 * (1 + 64) ) + keyvalue_size * (2 * (1 + 64)) + (account_size - 1) + 1 + 1;
        local<char> buffer_l(size); char *buffer = buffer_l.data;
        uint64_t offset = 0;
        for (uint64_t i = 0; i < strlen(cmd); i++) buffer[offset++] = cmd[i];
        buffer[offset++] = ' ';
        buffer[offset++] = '\'';
        for (uint64_t i = 0; i < account_size; i++) {
            if (account_list[i].nonce == 0 && account_list[i].balance == 0 && (account_list[i].codehash == 0 || account_list[i].codehash == EMPTY_CODEHASH)) continue;
            if (i > 0) buffer[offset++] = ';';
            uint160_t address = account_index[i];
            to_hex<160>(address, &buffer[offset]); offset += 40;
            buffer[offset++] = ',';
            to_hex<256>(account_list[i].nonce, &buffer[offset]); offset += 64;
            buffer[offset++] = ',';
            to_hex<256>(account_list[i].balance, &buffer[offset]); offset += 64;
            buffer[offset++] = ',';
            to_hex<256>(account_list[i].codehash, &buffer[offset]); offset += 64;
            for (uint64_t j = 0; j < keyvalue_size; j++) {
                if (address == account_index[keyvalue_index[i]]) {
                    if (keyvalue_list[j][1] == 0) continue;
                    buffer[offset++] = ',';
                    to_hex<256>(keyvalue_list[j][0], &buffer[offset]); offset += 64;
                    buffer[offset++] = ',';
                    to_hex<256>(keyvalue_list[j][1], &buffer[offset]); offset += 64;
                }
            }

        }
        buffer[offset++] = '\'';
        buffer[offset++] = '\0';
        assert(offset <= size);
        return exec(buffer);
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
