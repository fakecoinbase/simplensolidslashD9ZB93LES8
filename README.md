# EOSIO Challenge Submission

This is a "to-the-point" documentation. Please get in touch should you have any questions.

The submission implements all the functionality featured in the technical requirements.

## Basic Structure

The two relevant files regarding the EOSIO EVM contract are:

- [evm.hpp](src/evm.hpp) This is a portable self-contained EVM interpreter
- [evm.cpp](contracts/evm/evm.cpp) This is the contract carrying EOSIO specifics

these are self documented with comments and written with readability in mind.

Additional files are required for testing:

- [evm.cpp](src/evm.cpp) This is the standalone environment used as base for tests
- [tester.py](tests/tester.py) This is the script that coordinates the execution of tests
- [deploy.py](tests/sol/deploy.py) This is the script helps packaging EVM bytecode into unsigned transactions to be used in the raw action
- [WSYS.sol](tests/sol/WSYS.sol) This is the sample ERC-20 contract that implements Wrapped SYS (WSYS)

The test setup and procedure is documented in a [separate file](tests/README.md).

## Submission Notes

- The code is self contained and does not rely on additional libraries
- The final contract .wasm is arount 200Kb (compiled with -O=z). It produces a "deadline exceeded" error when submitted but works (it seems to be the case when the .wasm is beyond 190Kb)
- CHAIN_ID is hardcoded but can be modified by editting the interpreter [[evm.cpp](https://github.com/simplensolid/eosio-challenge-D9ZB93LES8/blob/5070afc9f55a86a544ad0f295410d130d0742bde/src/evm.hpp#L2720)]
- The implementation supports all releases of Ethereum based on the `forknumber()` [[evm.cpp](https://github.com/simplensolid/eosio-challenge-D9ZB93LES8/blob/5070afc9f55a86a544ad0f295410d130d0742bde/contracts/evm/evm.cpp#L301)] method implemented by the contract and the `releaseforkblock` table hardcoded into the interpreter [[evm.cpp](https://github.com/simplensolid/eosio-challenge-D9ZB93LES8/blob/5070afc9f55a86a544ad0f295410d130d0742bde/src/evm.hpp#L2737)]
- Current block number returned is the one provided by `eosio::tapos_block_num()`, as no other alternative was found in the EOSIO platform documentation
- All tests passed running in standalone mode (not as an EOSIO contract), except a small inconsistent set documented in [failing.txt](tests/failing.txt). For those, this implementation behaves as the reference implementation (`geth`)
- We had to modify `max-transaction-time` and `max_block_cpu_usage` in order to be able to run our ERC-20 test
- The bigint and ECDSA portions of this implementation are not optimized

## Performance issues

This submission does not optimize for performance. Instead it provides a clean and portable EVM implementation.

There are some opportunities for performance improvements in WASM. But getting a general EVM contract to run within the limits of an EOS transaction seems impossible without customizing the EOSIO software stack.

One of such customizations would be to provide the EVM precompiled contracts functionality natively as part of the EOSIO library.

