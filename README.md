# EOSIO Challenge Submission

This is a "to-the-point" documentation. Please get in touch should you have any questions.

The submission implements all the functionality featured in the technical requirements.

# Basic Structure

The two relevant files regarding the EOSIO EVM contract are:

- [evm.hpp](src/evm.hpp) This is a portable self-contained EVM interpreter
- [evm.cpp](contracts/evm/evm.cpp) This is the contract carrying EOSIO specifics

these are self documented with comments and written with readability in mind.

Additional files are required for testing:

- [evm.cpp](src/evm.cpp) This is the standalone environment used as base for tests
- [tester.py](tests/tester.py) This the script that coordinates the execution of tests
- [deploy.py](tests/sol/deploy.py) This the script helps packaging EVM bytecode into unsigned transactions to be used in the raw action
- [WSYS.sol](tests/sol/WSYS.sol) This is the sample ERC-20 contract that implements Wrapped SYS (WSYS)

The test setup and procedure is documented in a [separate file](tests/README.md).

## Observations

- The code is self contained and does not rely on additional libraries
- The final contract .wasm is arount 200kb (compiled with -O=z)
- CHAIN_ID is hardcoded but can be modified by editting the interpreter [[evm.cpp](https://github.com/simplensolid/eosio-challenge-D9ZB93LES8/blob/5070afc9f55a86a544ad0f295410d130d0742bde/src/evm.hpp#L2720)]
- The implementation supports all releases of Ethereum based on the $forknumber()$ [[evm.cpp](https://github.com/simplensolid/eosio-challenge-D9ZB93LES8/blob/5070afc9f55a86a544ad0f295410d130d0742bde/contracts/evm/evm.cpp#L301)] method implemented by the contract and the releaseforkblock table hardcoded into the interpreter [[evm.cpp](https://github.com/simplensolid/eosio-challenge-D9ZB93LES8/blob/5070afc9f55a86a544ad0f295410d130d0742bde/src/evm.hpp#L2737)]
- Current block number returned is the one provided by $eosio::tapos_block_num()$, as no other alternative was found in the EOSIO platform documentation
- All tests passed, except a small inconsistent set documented in [failing.txt](tests/failing.txt). For those, this implementation bahaves as the reference implementation ($geth$)

## Performance issues

The main issue with this submission is performance.

There is little hope that in a general setting the EVM will run over EOS without using one or many of the approaches below:

- increasing the EOSIO time and cpu usage limits
- implementing a single ETH transaction as multiple EOS deferred transactions
- augmenting the EOSIO code base to support the ECDSA precompiled contracts natively

This implementation did not put an effort to optimize performance of precompiled contracts and ECDSA.

Therefore the performance of these features are really slow and not suitable for a production environment.

But we can improve those.

