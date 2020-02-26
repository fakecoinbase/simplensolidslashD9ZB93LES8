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
        uint64_t tertiary_key() const { return id64(address); }
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
        uint64_t tertiary_key() const { return id64(acc_id, key); }
    };
    typedef eosio::indexed_by<"state3"_n, eosio::const_mem_fun<state_table, uint64_t, &state_table::tertiary_key>> state_tertiary;
    typedef eosio::indexed_by<"state2"_n, eosio::const_mem_fun<state_table, uint64_t, &state_table::secondary_key>> state_secondary;
    typedef eosio::multi_index<"state"_n, state_table, state_secondary, state_tertiary> state_index;
    state_index _state;

    // Account Code Table (per account):
    // - EVM bytecode associated with account
    struct [[eosio::table]] code_table {
        uint64_t code_id;
        uint64_t acc_id;
        std::vector<uint8_t> code;

        uint64_t primary_key() const { return code_id; }
        uint64_t secondary_key() const { return acc_id; }
        uint64_t tertiary_key() const { return id64(code); }
    };
    typedef eosio::indexed_by<"code3"_n, eosio::const_mem_fun<code_table, uint64_t, &code_table::tertiary_key>> code_tertiary;
    typedef eosio::indexed_by<"code2"_n, eosio::const_mem_fun<code_table, uint64_t, &code_table::secondary_key>> code_secondary;
    typedef eosio::multi_index<"code"_n, code_table, code_secondary, code_tertiary> code_index;
    code_index _code;

    // get account id from eos username integer id
    uint64_t get_account(uint64_t user_id) const {
        auto idx = _account.get_index<"account2"_n>();
        auto itr = idx.find(user_id);
        if (itr != idx.end()) return itr->acc_id;
        return 0;
    }

    // get account id from 160-bit address
    uint64_t get_account(const uint160_t &address) const {
        uint64_t key_id = id64(address);
        auto idx = _account.get_index<"account3"_n>();
        auto itr = idx.find(key_id);
        while (itr != idx.end()) {
            if (equals(itr->address, address)) return itr->acc_id;
        }
        return 0;
    }

    // get eos username integer id from 160-bit address
    uint64_t get_user_id(const uint160_t &address) const {
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            return itr->user_id;
        }
        return 0;
    }

    // get account balance, assumes account exists
    uint64_t get_balance(uint64_t acc_id) const {
        auto itr = _account.find(acc_id);
        return itr->balance;
    };

    // add to account balance, assumes account exists
    void add_balance(uint64_t acc_id, uint64_t amount) {
        auto itr = _account.find(acc_id);
        _account.modify(itr, _self, [&](auto& row) {
            check(row.balance + amount > row.balance, "account balance overflows");
            row.balance += amount;
        });
    }

    // sub from account balance, assumes account exists
    void sub_balance(uint64_t acc_id, uint64_t amount) {
        auto itr = _account.find(acc_id);
        _account.modify(itr, _self, [&](auto& row) {
            check(amount <= row.balance, "insufficient balance");
            row.balance -= amount;
        });
    }

    // insert a new account, assumes account does not exist (unique address/user_id)
    uint64_t insert_account(const uint160_t &address, uint64_t nonce, uint64_t balance, uint64_t user_id) {
        uint64_t acc_id = _max(1, _account.available_primary_key());
        _account.emplace(_self, [&](auto& row) {
            row.acc_id = acc_id;
            row.address = convert(address);
            row.nonce = nonce;
            row.balance = balance;
            row.user_id = user_id;
        });
        return acc_id;
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
    void raw(const string& data, const checksum160& _sender) {
        check(data.size() % 2 == 0, "hexadecimal string should have an even size");
        uint64_t size = data.size() / 2;
        uint8_t buffer[size];
        for (uint64_t i = 0; i < data.size(); i++) {
            char c = data[i];
            check(('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f'), "invalid hexadecimal character");
            uint64_t j = i/2;
            buffer[j] <<= 4;
            if (c >= 'a') { buffer[j] |= (uint8_t)(c - 'a' + 10); continue; }
            if (c >= 'A') { buffer[j] |= (uint8_t)(c - 'A' + 10); continue; }
            buffer[j] |= (uint8_t)(c - '0');
        }
        uint160_t sender = convert(_sender);
        struct txn txn = {0, 0, 0, false, 0, 0, nullptr, 0, false, 0, 0, 0};
        _try({
            _catches(decode_txn)(buffer, size, txn);
        }, Error e, {
            check(false, "malformed transaction");
        })
        check(txn.is_signed, "missing signature");
        bool eos_auth = txn.r == 0 && txn.s == 0 && sender > 0;
        if (!eos_auth) {
            // this is partially redundant with vm_txn, which is ok
            // transaction validation failure will happen early in here
            // sender recovery will be skipped there
            _try({
                Release release = get_release(forknumber());
                _catches(verify_txn)(release, txn);
                uint256_t h = _catches(hash_txn)(txn);
                sender = _catches(ecrecover)(h, txn.v, txn.r, txn.s);
            }, Error e, {
                check(false, "invalid transaction");
            })
        }
        uint64_t user_id = get_user_id(sender);
        check(user_id > 0, "unknown account");
        name account(user_id);
        if (eos_auth) require_auth(account);
        _try({
            bool pays_gas = false;
            _catches(vm_txn)(*this, *this, buffer, size, sender, pays_gas);
        }, Error e, {
            check(false, "execution failure: " + string(errors[e]));
        })
        eosio::print_f("info: transaction executed by % (%)\n", account, user_id);
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
        check(acc_id == 0, "user already has an account");
        string _name = account.to_string();
        const uint8_t *name = (const uint8_t*)_name.c_str();
        const uint8_t *data = (const uint8_t*)_data.c_str();
        uint160_t address;
        struct rlp rlp = { false, 0, nullptr };
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
        // the operation should create a new account table entry
        // therefore we need to fail if one does exist
        uint64_t _acc_id = get_account(address);
        check(_acc_id == 0, "this account already exists");
        insert_account(address, 1, 0, user_id);
        eosio::print_f("info: account created for % (%) at 0x%\n", account, user_id, to_string(address));
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
    void withdraw(const name& account, const asset& quantity) {
        require_auth(account);
        uint64_t user_id = account.value;
        uint64_t acc_id = get_account(user_id);
        check(acc_id > 0, "account does not exist");
        check(quantity.symbol == eosio::symbol("SYS", 4), "should withdraw SYS");
        uint64_t amount = quantity.amount;
        uint64_t balance = get_balance(acc_id);
        check(balance >= amount, "insufficient funds");
        sub_balance(acc_id, amount);
        struct transfer { eosio::name from; eosio::name to; eosio::asset quantity; std::string memo; };
        eosio::action transfer_action = eosio::action(
            eosio::permission_level(_self, "active"_n),
            "eosio.token"_n,
            "transfer"_n,
            transfer{ _self, account, quantity, "withdrawal" });
        transfer_action.send();
        eosio::print_f("info: withdrawal of % to % (%)\n", quantity, account, user_id);
    }

    // The Application MUST respond to EOSIO token transfers
    // - Provided that the EOSIO account in the “from” field of the transfer maps to a known and valid Account Table entry through the entry’s unique associated EOSIO account
    // - Transferred tokens should be added to the Account Table entry’s balance
    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(const name& account, const name &to, asset &quantity, string memo) {
        if (to == _self && quantity.symbol == eosio::symbol("SYS", 4)) {
            uint64_t user_id = account.value;
            uint64_t acc_id = get_account(user_id);
            if (acc_id > 0) {
                uint64_t amount = quantity.amount;
                add_balance(acc_id, amount);
                eosio::print_f("info: deposit of % from % (%)\n", quantity, account, user_id);
            }
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

    static constexpr uint64_t _forknumber = 10000000; // after istanbul
    static constexpr uint64_t _gaslimit = 10000000; // sufficiently large supply
    static constexpr uint64_t _difficulty = 17179869184; // aleatory, from genesis

    // vm callback to obtain the current ETH mainnet block number
    // based on this number the vm realizes the current release
    // and may run in compatibility mode
    uint64_t forknumber() {
        return _forknumber;
    }

    // vm call back to obtain the current block number
    uint64_t number() {
        return 0; // implement
    }

    // vm call back to obtain the current block timestamp
    uint64_t timestamp() {
        return eosio::current_block_time().to_time_point().sec_since_epoch();
    }

    // vm callback to obtain the current block gas limit
    uint64_t gaslimit() {
        return _gaslimit;
    }

    // vm callback to obtain the current block coinbase
    uint160_t coinbase() {
        return 0; // could be something else
    }

    // vm callback to obtain the current block difficulty
    uint256_t difficulty() {
        return _difficulty;
    }

    // vm callback to obtain the hash of a block
    uint256_t hash(const uint256_t &number) {
        uint8_t buffer[32];
        uint256_t::to(number, buffer);
        return sha3(buffer, 32);
    }

private:
    // vm callback to read the noce
    uint64_t get_nonce(const uint160_t &address) const {
        eosio::print_f("debug: get_nonce address<0x{%}>", to_string(address));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            return itr->nonce;
        }
        return 0;
    }

    // vm callback to update the noce
    void set_nonce(const uint160_t &address, const uint64_t &nonce) {
        eosio::print_f("debug: set_nonce address<0x{%}> value<0x{%}>", to_string(address), to_string((uint256_t)nonce));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            _account.modify(itr, _self, [&](auto& row) { row.nonce = nonce; });
            return;
        }
        if (nonce > 0) insert_account(address, nonce, 0, 0);
    }

    // vm callback to read the balance
    uint256_t get_balance(const uint160_t &address) const {
        eosio::print_f("debug: get_balance address<0x{%}>", to_string(address));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            return itr->balance;
        }
        return 0;
    };

    // vm callback to update the balance
    void set_balance(const uint160_t &address, const uint256_t &_balance) {
        eosio::print_f("debug: set_balance address<0x{%}> value<0x{%}>", to_string(address), to_string(_balance));
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

    // vm callback to read the account codehash
    uint256_t get_codehash(const uint160_t &address) const {
        eosio::print_f("debug: get_codehash address<0x{%}>", to_string(address));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto idx = _code.get_index<"code2"_n>();
            auto itr = idx.find(acc_id);
            if (itr != idx.end()) return id64(itr->code);
        }
        return 0;
    };

    // vm callback to update the account codehash
    void set_codehash(const uint160_t &address, const uint256_t &codehash) {
        eosio::print_f("debug: set_codehash address<0x{%}> value<0x{%}>", to_string(address), to_string(codehash));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto idx = _code.get_index<"code2"_n>();
            auto itr = idx.find(acc_id);
            if (itr != idx.end()) {
                if (id64(itr->code) == codehash) return;
                idx.erase(itr);
            }
        }
        if (codehash > 0) {
            if (acc_id == 0) acc_id = insert_account(address, 0, 0, 0);
            uint64_t hash_id = id64(codehash);
            auto idx = _code.get_index<"code3"_n>();
            auto itr = idx.find(hash_id);
            while (itr != idx.end()) {
                if (itr->acc_id == 0 && id64(itr->code) == codehash) {
                    idx.modify(itr, _self, [&](auto& row) { row.acc_id = acc_id; });
                    return;
                }
            }
        }
    };

    // vm call back to load code
    uint8_t *load_code(const uint256_t &codehash, uint64_t &code_size) const {
        eosio::print_f("debug: load_code codehash<0x{%}>", to_string(codehash));
        uint64_t hash_id = id64(codehash);
        auto idx = _code.get_index<"code3"_n>();
        auto itr = idx.find(hash_id);
        while (itr != idx.end()) {
            if (id64(itr->code) == codehash) {
                code_size = itr->code.size();
                uint8_t *code = _new<uint8_t>(code_size);
                for (uint64_t i = 0; i < code_size; i++) code[i] = itr->code[i];
                return code;
            }
        }
        code_size = 0;
        return nullptr;
    };

    // vm call back to store code
    void store_code(const uint256_t &codehash, const uint8_t *code, uint64_t code_size) {
        eosio::print_f("debug: store_code codehash<0x{%}> value<0x{%}>", to_string(codehash), to_string(code, code_size));
        uint64_t hash_id = id64(codehash);
        auto idx = _code.get_index<"code3"_n>();
        auto itr = idx.find(hash_id);
        while (itr != idx.end()) {
            if (id64(itr->code) == codehash) return;
        }
        _code.emplace(_self, [&](auto& row) {
            row.code_id = _max(1, _code.available_primary_key());
            row.acc_id = 0;
            row.code.resize(code_size);
            for (uint64_t i = 0; i < code_size; i++) row.code[i] = code[i];
        });
    };

    // vm callback to read from the storage
    uint256_t load(const uint160_t &address, const uint256_t &key) const {
        eosio::print_f("debug: load address<0x{%}> key<0x{%}>", to_string(address), to_string(key));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            uint64_t key_id = id64(acc_id, key);
            auto idx = _state.get_index<"state3"_n>();
            auto itr = idx.find(key_id);
            while (itr != idx.end()) {
                if (itr->acc_id == acc_id && equals(itr->key, key)) return convert(itr->value);
            }
        }
        return 0;
    };

    // vm callback to update the storage
    void store(const uint160_t &address, const uint256_t &key, const uint256_t& value) {
        eosio::print_f("debug: store address<0x{%}> key<0x{%}> value<0x{%}>", to_string(address), to_string(key), to_string(value));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            uint64_t key_id = id64(acc_id, key);
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
        }
        if (value > 0) {
            if (acc_id == 0) acc_id = insert_account(address, 0, 0, 0);
            _state.emplace(_self, [&](auto& row) {
                row.key_id = _max(1, _state.available_primary_key());
                row.acc_id = acc_id;
                row.key = convert(key);
                row.value = convert(value);
            });
        }
    };

    // vm call back to clean up accounts
    void remove(const uint160_t &address) {
        eosio::print_f("debug: remove address<{0x%}>", to_string(address));
        uint64_t acc_id = get_account(address);
        if (acc_id > 0) {
            auto itr = _account.find(acc_id);
            if (itr->user_id == 0) _account.erase(itr);
        }
    }

    // vm call back to register log0
    void log0(const uint160_t &address, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log0 address<0x{%}> data<0x{%}>\n",
            to_string(address), to_string(data, data_size));
    }

    // vm call back to register log1
    void log1(const uint160_t &address, const uint256_t &topic1, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log1 address<0x{%}> topic1<0x{%}> data<0x{%}>\n",
            to_string(address), to_string(topic1), to_string(data, data_size));
    }

    // vm call back to register log2
    void log2(const uint160_t &address, const uint256_t &topic1, const uint256_t &topic2, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log2 address<0x{%}> topic1<0x{%}> topic2<0x{%}> data<0x{%}>\n",
            to_string(address), to_string(topic1), to_string(topic2), to_string(data, data_size));
    }

    // vm call back to register log3
    void log3(const uint160_t &address, const uint256_t &topic1, const uint256_t &topic2, const uint256_t &topic3, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log3 address<0x{%}> topic1<0x{%}> topic2<0x{%}> topic3<0x{%}> data<0x{%}>\n",
            to_string(address), to_string(topic1), to_string(topic2), to_string(topic3), to_string(data, data_size));
    }

    // vm call back to register log4
    void log4(const uint160_t &address, const uint256_t &topic1, const uint256_t &topic2, const uint256_t &topic3, const uint256_t &topic4, const uint8_t *data, uint64_t data_size) {
        eosio::print_f("log4 address<0x{%}> topic1<0x{%}> topic2<0x{%}> topic3<0x{%}> topic4<0x{%}> data<0x{%}>\n",
            to_string(address), to_string(topic1), to_string(topic2), to_string(topic3), to_string(topic4), to_string(data, data_size));
    }

private:

    // generates a low collision 64-bit id for 160-bit addresses
    static uint64_t id64(const checksum160 &address) {
        auto _address = address.extract_as_byte_array();
        return 0
            | (uint64_t)_address[12] << 56
            | (uint64_t)_address[13] << 48
            | (uint64_t)_address[14] << 40
            | (uint64_t)_address[15] << 32
            | (uint64_t)_address[16] << 24
            | (uint64_t)_address[17] << 16
            | (uint64_t)_address[18] << 8
            | (uint64_t)_address[19];
    }

    // generates a low collision 64-bit id for 160-bit addresses
    static uint64_t id64(const uint160_t &address) {
        return 0
            | (uint64_t)address.byte(7) << 56
            | (uint64_t)address.byte(6) << 48
            | (uint64_t)address.byte(5) << 40
            | (uint64_t)address.byte(4) << 32
            | (uint64_t)address.byte(3) << 24
            | (uint64_t)address.byte(2) << 16
            | (uint64_t)address.byte(1) << 8
            | (uint64_t)address.byte(0);
    }

    // generates a low collision 64-bit id for 64/256-bit acc_id/keys
    static uint64_t id64(uint64_t acc_id, const checksum256 &key) {
        return id64(convert(key));
    }

    // generates a low collision 64-bit id for 64/256-bit acc_id/keys
    static uint64_t id64(uint64_t acc_id, const uint256_t &key) {
        return acc_id ^ key.murmur3(acc_id);
    }

    // generates a low collision 64-bit id for bytecode
    static uint64_t id64(const std::vector<uint8_t> &code) {
        uint64_t size = code.size();
        uint8_t buffer[size];
        for (uint64_t i = 0; i < size; i++) buffer[i] = code[i];
        return id64(sha3(buffer, size));
    }

    // generates a low collision 64-bit id for codehash
    static uint64_t id64(const uint256_t &codehash) {
        return 0
            | (uint64_t)codehash.byte(7) << 56
            | (uint64_t)codehash.byte(6) << 48
            | (uint64_t)codehash.byte(5) << 40
            | (uint64_t)codehash.byte(4) << 32
            | (uint64_t)codehash.byte(3) << 24
            | (uint64_t)codehash.byte(2) << 16
            | (uint64_t)codehash.byte(1) << 8
            | (uint64_t)codehash.byte(0);
    }

    // compare checksum160 for equality
    static bool equals(const checksum160 &v1, const uint160_t &v2) {
        auto _v1 = v1.extract_as_byte_array();
        for (uint64_t i = 0; i < 20; i++) if (_v1[i] == v2.byte(19 - i)) return true;
        return false;
    }

    // compare checksum256 for equality
    static bool equals(const checksum256 &v1, const uint256_t &v2) {
        auto _v1 = v1.extract_as_byte_array();
        for (uint64_t i = 0; i < 32; i++) if (_v1[i] == v2.byte(31 - i)) return true;
        return false;
    }

    // conversion from checksum160
    static uint160_t convert(const checksum160 &v) {
        uint160_t t;
        auto c = v.extract_as_byte_array();
        for (uint64_t i = 0; i < 20; i++) t.setbyte(19 - i, c[i]);
        return t;
    };

    // conversion to checksum160
    static checksum160 convert(const uint160_t &v) {
        std::array<uint8_t, 20> c;
        for (uint64_t i = 0; i < 20; i++) c[i] = v.byte(19 - i);
        return checksum160(c);
    };

    // conversion from checksum256
    static uint256_t convert(const checksum256 &v) {
        uint256_t t;
        auto c = v.extract_as_byte_array();
        for (uint64_t i = 0; i < 32; i++) t.setbyte(31 - i, c[i]);
        return t;
    };

    // conversion to checksum256
    static checksum256 convert(const uint256_t &v) {
        std::array<uint8_t, 32> c;
        for (uint64_t i = 0; i < 32; i++) c[i] = v.byte(31 - i);
        return checksum256(c);
    };

    // conversion to hex string for printing
    static string to_string(const uint160_t &address) {
        uint8_t buffer[20];
        uint160_t::to(address, buffer);
        return to_string(buffer, 20);
    }

    // conversion to hex string for printing
    static string to_string(const uint256_t &value) {
        uint8_t buffer[32];
        uint256_t::to(value, buffer);
        return to_string(buffer, 32);
    }

    // conversion to hex string for printing
    static string to_string(const uint8_t *data, uint64_t size) {
        static char hex[] = "0123456789abcdef";
        char buffer[2* size];
        for (uint64_t i = 0; i < size; i++) {
            buffer[2*i] = hex[data[i] >> 4];
            buffer[2*i+1] = hex[data[i] & 0xf];
        }
        return string(buffer, buffer + 2*size);
    }

};
