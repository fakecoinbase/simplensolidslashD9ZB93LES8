#include <eosio/eosio.hpp>

// EVM executes as the Yellow Paper, and:
// - there will be no effective BLOCK gas limit. Instructions that return block limit should return a sufficiently large supply
// - the TRANSACTION gas limit will be enforced
// - the sender WILL NOT be billed for the gas, the gas price MAY therefore be locked at some suitable value
// - all other gas mechanics/instructions should be maintained
// - block number and timestamp should represent the native EOSIO block number and time
// - block hash, coinbase, and difficulty should return static values

// The Application MUST respond to EOSIO token transfers
// - Provided that the EOSIO account in the “from” field of the transfer maps to a known and valid Account Table entry through the entry’s unique associated EOSIO account
// - Transferred tokens should be added to the Account Table entry’s balance

using namespace eosio;

using std::string;

class [[eosio::contract("evm")]] evm : public contract {
private:

    // Account Table
    struct [[eosio::table]] account_table {
        uint8_t account_id[20]; // - a unique 160bit account ID
        uint64_t nonce; // - a nonce (sequence number)
        uint128_t token_balance; // - an EOSIO token balance (aka SYS)
        name account; // - [optional] a unique associated EOSIO account

        uint64_t primary_key() const { return 0; }
        uint64_t get_secondary() const { return account.value; }
    };

    // Account State Table (per account)
    struct [[eosio::table]] account_state_table {
        int8_t key[32]; // - a unique 256bit key
        int8_t value[32]; // - a 256bit value
    };

    // Account Code Table (per account)
    struct [[eosio::table]] account_code_table {
        int8_t bytecode[]; // - EVM bytecode associated with account
    };

public:
    using contract::contract;

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
    void raw(const string& data, const checksum160 sender) {
        print("Unimplemented");
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
    void withdraw(const name& account, const uint128_t amount) {
        require_auth(account);
        print("Unimplemented");
    }

    // The Application MAY implement additional actions for maintenance or
    // transaction processing so long as they do not violate the spirit of the
    // execution model above.

    // The Application MUST implement some method of specifying the “CHAIN_ID” for EIP-155 compatibility.
    // - this MAY be done at compile time
    // - this MAY be done with an additional initialization action
};
