#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

#include "../../src/evm.hpp"

using namespace eosio;

using std::string;

class [[eosio::contract("evm")]] evm : public contract, public Block, public State {
private:

    struct [[eosio::table]] account_table {
        uint64_t id;
        checksum160 address;
        uint64_t nonce;
        uint64_t balance;
        uint64_t user_id;

        uint64_t primary_key() const { return id; }
        uint64_t secondary_key() const { return user_id; }
        uint64_t tertiary_key() const { return hash(address); }
    };
    typedef eosio::indexed_by<"account3"_n, eosio::const_mem_fun<account_table, uint64_t, &account_table::tertiary_key>> account_tertiary;
    typedef eosio::indexed_by<"account2"_n, eosio::const_mem_fun<account_table, uint64_t, &account_table::secondary_key>> account_secondary;
    typedef eosio::multi_index<"account"_n, account_table, account_secondary, account_tertiary> account_index;
    account_index _account;

    struct [[eosio::table]] state_table {
        uint64_t id;
        uint64_t acc_id;
        checksum256 key;
        checksum256 value;

        uint64_t primary_key() const { return id; }
        uint64_t secondary_key() const { return acc_id; }
        uint64_t tertiary_key() const { return hash(acc_id, key); }
    };
    typedef eosio::indexed_by<"state3"_n, eosio::const_mem_fun<state_table, uint64_t, &state_table::tertiary_key>> state_tertiary;
    typedef eosio::indexed_by<"state2"_n, eosio::const_mem_fun<state_table, uint64_t, &state_table::secondary_key>> state_secondary;
    typedef eosio::multi_index<"state"_n, state_table, state_secondary, state_tertiary> state_index;
    state_index _state;

    struct [[eosio::table]] code_table {
        uint64_t id;
        uint64_t acc_id;
        std::vector<uint8_t> code;

        uint64_t primary_key() const { return id; }
        uint64_t secondary_key() const { return acc_id; }
        uint64_t tertiary_key() const { return hash(code); }
    };
    typedef eosio::indexed_by<"code3"_n, eosio::const_mem_fun<code_table, uint64_t, &code_table::tertiary_key>> code_tertiary;
    typedef eosio::indexed_by<"code2"_n, eosio::const_mem_fun<code_table, uint64_t, &code_table::secondary_key>> code_secondary;
    typedef eosio::multi_index<"code"_n, code_table, code_secondary, code_tertiary> code_index;
    code_index _code;

    static uint64_t hash(const uint160_t &address) { return 0; } // implement
    static uint64_t hash(const checksum160 &address) { return 0; } // implement
    static uint64_t hash(uint64_t acc_id, const checksum256 &key) { return 0; } // implement
    static uint64_t hash(uint64_t acc_id, const uint256_t &key) { return 0; } // implement
    static uint64_t hash(const std::vector<uint8_t> &code) { return 0; } // implement
    static uint64_t _hash(const uint256_t &codehash) { return 0; } // implement
    static bool equals(const checksum160 &address1, const uint160_t &address2) { return false; } // implement
    static bool equals(const checksum256 &key1, const uint256_t &key2) { return false; } // implement
    static checksum160 convert(const uint160_t &address) { abort(); };
    static uint256_t convert(const checksum256 &address) { abort(); };
    static checksum256 convert(const uint256_t &address) { abort(); };

public:
    using contract::contract;

    evm(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
        _account(receiver, receiver.value),
        _state(receiver, receiver.value),
        _code(receiver, receiver.value) {}

    // 1 a binary Ethereum transaction encoded as it appears in a serialized Ethereum block
    // 2 [optional] A 160bit account identifier “Sender”
    // results:
    // - Appropriate Updates to Account, Account State, and Account Code Tables reflecting the application of the transaction
    // - Log output (via EOSIO print intrinsics)
    // if the “R” and “S” values of the transaction are NOT 0:
    // - a transaction containing this action must fail if the signature (V, R, S) within the input does not recover to a valid and known 160bit account identifier in the Accounts Table
    // if the “R” and “S” values of the transaction are 0:
    // - A transaction containing this action must fail if “Sender” input parameter is not present or does not refer to a valid and known 160bit account identifier in the Accounts Table
    // - If the associated entry in the Accounts Table has no Associated EOSIO Account
    // - OR if the transaction has not been authorized by the Associated EOSIO Account
    [[eosio::action]]
    void raw(const string& data, const checksum160 &sender) {
//        require_auth(account);
        _try({
            _catches(vm_txn)(*this, *this, nullptr, 0, 0);
        }, Error e, {
//            print("error");
        })
    }

    // 1 an EOSIO account
    // 2 an arbitrary length string
    // results, a new Account Table entry with:
    // Balance = 0
    // Nonce = 1
    // Account identifier = the rightmost 160 bits of the Keccak hash of the RLP encoding of the structure containing only the EOSIO account name and the arbitrary input string
    // requirements:
    // - a transaction containing this action must fail if it is not authorized by the EOSIO account listed in the inputs
    // - a transaction containing this action must fail if an Account Table entry exists with this EOSIO account associated
    [[eosio::action]]
    void create(const name& account, const string& data) {
        require_auth(account);

//        uint160_t t = (uint160_t)sha3((const uint8_t*)data.c_str(), data.length());
//        checksum160 c;
//        auto bytes = c.extract_as_byte_array();

// sha256(const_cast<char*>(str.c_str()), str.size(), &sum);
//        auto itr = testtab.find(user.value);
//        check(itr == testtab.end(), "account already exists");

        print("Unimplemented");
    }

    // 1 an EOSIO account
    // 2 a token amount
    // results:
    // - deducting the amount from the associated Account Table entry’s balance
    // - sending an inline EOSIO token transfer for the amount to the EOSIO account
    // - a transaction containing this action must fail if it is not authorized
    //   by the EOSIO account listed in the inputs OR if such a withdrawal would
    //   leave the Account Table entry’s balance negative
    [[eosio::action]]
    void withdraw(const name& account, const uint64_t amount) {
        require_auth(account);
//        check( , "user does not exist in table" );
    }

private:

    static constexpr uint64_t _gaslimit = 10000000; // sufficiently large supply
    static constexpr uint64_t _difficulty = 17179869184; // aleatory, from genesis

    uint160_t _coinbase = 0; // static value

    uint64_t timestamp() {
        return eosio::current_block_time().to_time_point().sec_since_epoch();
    }

    uint64_t number() {
        return 0; // implement
    }

    uint64_t forknumber() {
        return 0; //implement
    }

    uint64_t gaslimit() {
        return _gaslimit;
    }

    uint64_t difficulty() {
        return _difficulty;
    }

    const uint160_t& coinbase() {
        static uint160_t _coinbase = 0; // could be something else
        return _coinbase;
    }

    uint256_t hash(const uint256_t &number) {
        uint8_t buffer[32];
        uint256_t::to(number, buffer);
        return sha3(buffer, 32);
    }

private:

    inline uint64_t get_account(const uint160_t &address) const {
        auto idx = _account.get_index<"account3"_n>();
        auto itr = idx.find(hash(address));
        while (itr != idx.end()) {
            if (equals(itr->address, address)) return itr->id;
        }
        return 0;
    }

    inline void insert_account(const uint160_t &address, uint64_t nonce, uint64_t balance, uint64_t user_id) {
        _account.emplace(_self, [&](auto& row) {
            row.id = _account.available_primary_key();
            row.address = convert(address);
            row.nonce = nonce;
            row.balance = balance;
            row.user_id = user_id;
        });
    }

    inline uint64_t get_nonce(const uint160_t &address) const {
        uint64_t id = get_account(address);
        if (id > 0) {
            auto itr = _account.find(id);
            return itr->nonce;
        }
        return 0;
    }

    inline void set_nonce(const uint160_t &address, const uint64_t &nonce) {
        uint64_t id = get_account(address);
        if (id > 0) {
            auto itr = _account.find(id);
            _account.modify(itr, _self, [&](auto& row) { row.nonce = nonce; });
            return;
        }
        if (nonce > 0) insert_account(address, nonce, 0, 0);
    }

    inline uint256_t get_balance(const uint160_t &address) const {
        uint64_t id = get_account(address);
        if (id > 0) {
            auto itr = _account.find(id);
            return itr->balance;
        }
        return 0;
    };

    inline void set_balance(const uint160_t &address, const uint256_t &_balance) {
        uint64_t balance = 0; // implement
        uint64_t id = get_account(address);
        if (id > 0) {
            auto itr = _account.find(id);
            _account.modify(itr, _self, [&](auto& row) { row.balance = balance; });
            return;
        }
        if (balance > 0) insert_account(address, balance, 0, 0);
    };

    inline uint256_t get_codehash(const uint160_t &address) const {
        uint64_t id = get_account(address);
        auto idx = _code.get_index<"code2"_n>();
        auto itr = idx.find(id);
        if (itr != idx.end()) return hash(itr->code);
        return 0;
    };

    inline void set_codehash(const uint160_t &address, const uint256_t &codehash) {
        //_codehash.set(address, codehash, 0);
    };

    inline const uint8_t *load_code(const uint256_t &codehash, uint64_t &code_size) const {
        auto idx = _code.get_index<"code3"_n>();
        auto itr = idx.find(_hash(codehash));
        while (itr != idx.end()) {
            // if (sha3(itr->code) == codehash) return 0; // implement
        }
        return 0;
    };

    inline void store_code(const uint256_t &codehash, const uint8_t *code, uint64_t code_size) {
        // implement
    };

    inline uint256_t load(const uint160_t &address, const uint256_t &key) const {
        uint64_t id = get_account(address);
        auto idx = _state.get_index<"state3"_n>();
        auto itr = idx.find(hash(id, key));
        while (itr != idx.end()) {
            if (itr->acc_id == id && equals(itr->key, key)) return convert(itr->value);
        }
        return 0;
    };

    inline void store(const uint160_t &address, const uint256_t &key, const uint256_t& value) {
        uint64_t id = get_account(address);
        auto idx = _state.get_index<"state3"_n>();
        auto itr = idx.find(hash(id, key));
        while (itr != idx.end()) {
            if (itr->acc_id == id && equals(itr->key, key)) {
                idx.modify(itr, _self, [&](auto& row) { row.value = convert(value); });
                return;
            }
        }
        if (value > 0) {
            _state.emplace(_self, [&](auto& row) {
                row.id = _account.available_primary_key();
                row.acc_id = id;
                row.key = convert(key);
                row.value = convert(value);
            });
        }
    };

    inline void remove(const uint160_t &address) {
        uint64_t id = get_account(address);
        if (id > 0) {
            auto itr = _account.find(id);
            _account.erase(itr);
        }
    }

    inline void log0(const uint160_t &address, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log {%}\n", 0); // implement
    }

    inline void log1(const uint160_t &address, const uint256_t &v1, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log {%}\n", 1); // implement
    }

    inline void log2(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log {%}\n", 2); // implement
    }

    inline void log3(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log {%}\n", 3); // implement
    }

    inline void log4(const uint160_t &address, const uint256_t &v1, const uint256_t &v2, const uint256_t &v3, const uint256_t &v4, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log {%}\n", 4); // implement
    }

};
