#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

#include "../../src/evm.hpp"

using namespace eosio;

using std::string;

class [[eosio::contract("evm")]] evm : public contract, public Block, public State {
private:

    // Account Table:
    // - a unique 160bit account ID
    // - a nonce (sequence number)
    // - an EOSIO token balance (aka SYS)
    // - [optional] a unique associated EOSIO account
    struct [[eosio::table]] account_table {
        uint64_t acc_id;
        checksum160 address;
        uint64_t nonce;
        uint64_t balance;
        uint64_t user_id;

        uint64_t primary_key() const { return acc_id; }
        uint64_t secondary_key() const { return user_id; }
        uint64_t tertiary_key() const { return hash(address); }
    };
    typedef eosio::indexed_by<"account3"_n, eosio::const_mem_fun<account_table, uint64_t, &account_table::tertiary_key>> account_tertiary;
    typedef eosio::indexed_by<"account2"_n, eosio::const_mem_fun<account_table, uint64_t, &account_table::secondary_key>> account_secondary;
    typedef eosio::multi_index<"account"_n, account_table, account_secondary, account_tertiary> account_index;
    account_index _account;

    // Account State Table (per account):
    // - a unique 256bit key
    // - a 256bit value
    struct [[eosio::table]] state_table {
        uint64_t key_id;
        uint64_t acc_id;
        checksum256 key;
        checksum256 value;

        uint64_t primary_key() const { return key_id; }
        uint64_t secondary_key() const { return acc_id; }
        uint64_t tertiary_key() const { return hash(acc_id, key); }
    };
    typedef eosio::indexed_by<"state3"_n, eosio::const_mem_fun<state_table, uint64_t, &state_table::tertiary_key>> state_tertiary;
    typedef eosio::indexed_by<"state2"_n, eosio::const_mem_fun<state_table, uint64_t, &state_table::secondary_key>> state_secondary;
    typedef eosio::multi_index<"state"_n, state_table, state_secondary, state_tertiary> state_index;
    state_index _state;

    // Account Code Table (per account):
    // - EVM bytecode associated with account
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

    static inline uint64_t hash(const uint160_t &address) { return 0; } // implement
    static inline uint64_t hash(const checksum160 &address) { return 0; } // implement
    static inline uint64_t hash(uint64_t acc_id, const checksum256 &key) { return 0; } // implement
    static inline uint64_t hash(uint64_t acc_id, const uint256_t &key) { return 0; } // implement
    static inline uint64_t hash(const std::vector<uint8_t> &code) { return 0; } // implement
    static inline uint64_t _hash(const uint256_t &codehash) { return 0; } // implement
    static inline bool equals(const checksum160 &address1, const uint160_t &address2) { return false; } // implement
    static inline bool equals(const checksum256 &key1, const uint256_t &key2) { return false; } // implement
    static inline uint160_t convert(const checksum160 &address) { abort(); };
    static inline checksum160 convert(const uint160_t &address) { abort(); };
    static inline uint256_t convert(const checksum256 &address) { abort(); };
    static inline checksum256 convert(const uint256_t &address) { abort(); };

//        checksum160 c;
//        auto bytes = c.extract_as_byte_array();

    // get account id from eos username integer id
    inline uint64_t get_account(uint64_t user_id) const {
        auto idx = _account.get_index<"account2"_n>();
        auto itr = idx.find(user_id);
        if (itr != idx.end()) return itr->acc_id;
        return 0;
    }

    // get account id from 160-bit address
    inline uint64_t get_account(const uint160_t &address) const {
        auto idx = _account.get_index<"account3"_n>();
        auto itr = idx.find(hash(address));
        while (itr != idx.end()) {
            if (equals(itr->address, address)) return itr->acc_id;
        }
        return 0;
    }

    // get eos username integer id from 160-bit address
    inline uint64_t get_user_id(const uint160_t &address) const {
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            return itr->user_id;
        }
        return 0;
    }

    // get account balance, assumes account exists
    inline uint64_t get_balance(uint64_t acc_id) const {
        auto itr = _account.find(acc_id);
        return itr->balance;
    };

    // add to account balance, assumes account exists
    inline void add_balance(uint64_t acc_id, uint64_t amount) {
        auto itr = _account.find(acc_id);
        _account.modify(itr, _self, [&](auto& row) { row.balance += amount; });
    }

    // sub from account balance, assumes account exists and has enough balance
    inline void sub_balance(uint64_t acc_id, uint64_t amount) {
        auto itr = _account.find(acc_id);
        _account.modify(itr, _self, [&](auto& row) { row.balance -= amount; });
    }

    // insert a new account, assumer account does not exist (unique address/user_id)
    inline void insert_account(const uint160_t &address, uint64_t nonce, uint64_t balance, uint64_t user_id) {
        _account.emplace(_self, [&](auto& row) {
            row.acc_id = _account.available_primary_key();
            row.address = convert(address);
            row.nonce = nonce;
            row.balance = balance;
            row.user_id = user_id;
        });
    }

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
    void raw(const string& data, const checksum160 &_sender) {
        const uint8_t *buffer = (const uint8_t*)data.c_str();
        uint64_t size = data.size();
        uint160_t sender = convert(_sender);
        struct txn txn;
        _try({
            _catches(decode_txn)(buffer, size, txn);
        }, Error e, {
            check(false, "malformed transaction");
        })
        if (txn.r == 0 && txn.s == 0 && sender > 0) {
            uint64_t user_id = get_user_id(sender);
            name account(user_id);
            require_auth(account);
        }
        _try({
            bool pays_gas = false;
            _catches(vm_txn)(*this, *this, buffer, size, sender, pays_gas);
        }, Error e, {
            check(false, "execution failure: " + string(errors[e]));
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
    void create(const name& account, const string& _data) {
        require_auth(account);
        uint64_t user_id = account.value;
        uint64_t acc_id = get_account(user_id);
        check(acc_id == 0, "account already exists");
        string _name = account.to_string();
        const uint8_t *name = (const uint8_t*)_name.c_str();
        const uint8_t *data = (const uint8_t*)_data.c_str();
        uint160_t address;
        struct rlp rlp;
        _try({
            rlp.is_list = true;
            rlp.size = 2;
            rlp.list = _new<struct rlp>(rlp.size);
            rlp.list[0].is_list = false;
            rlp.list[0].size = _name.size();
            rlp.list[0].data = _new<uint8_t>(rlp.list[0].size);
            for (uint64_t i = 0; i < rlp.list[0].size; i++) rlp.list[0].data[i] = name[i];
            rlp.list[1].is_list = false;
            rlp.list[1].size = _data.size();
            rlp.list[1].data = _new<uint8_t>(rlp.list[1].size);
            for (uint64_t i = 0; i < rlp.list[1].size; i++) rlp.list[1].data[i] = data[i];
            uint64_t size = _catches(dump_rlp)(rlp, nullptr, 0);
            uint8_t buffer[size];
            _catches(dump_rlp)(rlp, buffer, size);
            free_rlp(rlp);
            address = (uint160_t)sha3(buffer, size);
        }, Error e, {
            free_rlp(rlp);
            check(false, "execution failure: " + string(errors[e]));
        })
        uint64_t _acc_id = get_account(address);
        check(_acc_id == 0, "account already exists");
        insert_account(address, 1, 0, user_id);
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
        uint64_t user_id = account.value;
        uint64_t acc_id = get_account(user_id);
        check(acc_id != 0, "account does not exist");
        uint64_t balance = get_balance(acc_id);
        check(balance >= amount, "insufficient funds");
        sub_balance(acc_id, amount);
        // perform SYS transfer of amount to account
    }

    // The Application MUST respond to EOSIO token transfers
    // - Provided that the EOSIO account in the “from” field of the transfer maps to a known and valid Account Table entry through the entry’s unique associated EOSIO account
    // - Transferred tokens should be added to the Account Table entry’s balance
    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(const name& account, const name &to, asset &quantity, string memo) {
        if (to == _self) {
//            if (quantity.symbol == ) {
//            add_balance(quantity.amount);
//            }
        }
    }

private:
    // EVM executes as the Yellow Paper, and:
    // - there will be no effective BLOCK gas limit. Instructions that return block limit should return a sufficiently large supply
    // - the TRANSACTION gas limit will be enforced
    // - the sender WILL NOT be billed for the gas, the gas price MAY therefore be locked at some suitable value
    // - all other gas mechanics/instructions should be maintained
    // - block number and timestamp should represent the native EOSIO block number and time
    // - block hash, coinbase, and difficulty should return static values

    static constexpr uint64_t _gaslimit = 10000000; // sufficiently large supply
    static constexpr uint64_t _difficulty = 17179869184; // aleatory, from genesis

    // vm call back to obtain the current block timestamp
    inline uint64_t timestamp() {
        return eosio::current_block_time().to_time_point().sec_since_epoch();
    }

    // vm call back to obtain the current block number
    inline uint64_t number() {
        return 0; // implement
    }

    // vm callback to obtain the current ETH mainnet block number
    // based on this number the vm realizes the current release
    // and may run in compatibility mode
    inline uint64_t forknumber() {
        return 0; //implement
    }

    // vm callback to obtain the current block gas limit
    inline uint64_t gaslimit() {
        return _gaslimit;
    }

    // vm callback to obtain the current block difficulty
    inline uint64_t difficulty() {
        return _difficulty;
    }

    // vm callback to obtain the current block coinbase
    inline const uint160_t coinbase() {
        return 0; // could be something else
    }

    // vm callback to obtain the hash of a block
    inline uint256_t hash(const uint256_t &number) {
        uint8_t buffer[32];
        uint256_t::to(number, buffer);
        return sha3(buffer, 32);
    }

private:
    // vm callback to read the noce
    inline uint64_t get_nonce(const uint160_t &address) const {
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            return itr->nonce;
        }
        return 0;
    }

    // vm callback to update the noce
    inline void set_nonce(const uint160_t &address, const uint64_t &nonce) {
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            _account.modify(itr, _self, [&](auto& row) { row.nonce = nonce; });
            return;
        }
        if (nonce > 0) insert_account(address, nonce, 0, 0);
    }

    // vm callback to read the balance
    inline uint256_t get_balance(const uint160_t &address) const {
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            return itr->balance;
        }
        return 0;
    };

    // vm callback to update the balance
    inline void set_balance(const uint160_t &address, const uint256_t &_balance) {
        check(_balance < ((uint256_t)1 << 64), "illegal state, invalid balance");
        uint64_t balance = _balance.cast64();
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
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

    // vm callback to read from the storage
    inline uint256_t load(const uint160_t &address, const uint256_t &key) const {
        uint64_t acc_id = get_account(address);
        uint64_t key_id = hash(acc_id, key);
        auto idx = _state.get_index<"state3"_n>();
        auto itr = idx.find(key_id);
        while (itr != idx.end()) {
            if (itr->acc_id == acc_id && equals(itr->key, key)) return convert(itr->value);
        }
        return 0;
    };

    // vm callback to update the storage
    inline void store(const uint160_t &address, const uint256_t &key, const uint256_t& value) {
        uint64_t acc_id = get_account(address);
        uint64_t key_id = hash(acc_id, key);
        auto idx = _state.get_index<"state3"_n>();
        auto itr = idx.find(key_id);
        while (itr != idx.end()) {
            if (itr->acc_id == acc_id && equals(itr->key, key)) {
                if (value > 0) {
                    idx.modify(itr, _self, [&](auto& row) { row.value = convert(value); });
                } else {
                    idx.erase(itr);
                }
                return;
            }
        }
        if (value > 0) {
            _state.emplace(_self, [&](auto& row) {
                row.key_id = _account.available_primary_key();
                row.acc_id = acc_id;
                row.key = convert(key);
                row.value = convert(value);
            });
        }
    };

    // vm call back to clean up accounts
    inline void remove(const uint160_t &address) {
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            if (itr->user_id == 0) _account.erase(itr);
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
