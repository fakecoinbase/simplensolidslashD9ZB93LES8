## EOSIO Challenge Submission

_This documentation is concise on purpose. Please get in touch should you have any questions._

The submission implements all the functionality featured in the Technical Requirements and passes all relevant tests in the test suite.

**If it does not satisfy any judging criteria, a short report describing what needs to be fixed is very much appreciated.**

### Submission Notes

- The code implements an EVM interpreter
- It is self-contained and does not rely on additional libraries
- We have modified EOSIO/eos and EOSIO/eosio.cdt to support keccak256 and other cryptographic primitives natively
- We have increased EOSIO/eos `maximum_call_depth` in order to accommodate the EVM call stack limit of 1024
- The curve BN256 code is not optmized for production and could run significantly faster if optimized

Regarding the technical requirements:

- Current block number returned is the one provided by `eosio::tapos_block_num()`, as no other alternative was found in the EOSIO platform documentation
- CHAIN_ID is hardcoded but can be modified by editting the interpreter [[evm.hpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/src/evm.hpp#L2766)]
- The implementation supports all releases of Ethereum based on the `forknumber()` method implemented by the contract [[evm.cpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/contracts/evm/evm.cpp#L591)] and the `releaseforkblock` table hardcoded into the interpreter [[evm.cpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/src/evm.hpp#L2784)]
- The current version of the `raw` action always check is the sender address has an EOSIO account associated, to relax this requirement for signed transactions please comment line 448 of [evm.cpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/contracts/evm/evm.cpp#L448)
- One can query an EVM account contents using the `inspect` action which takes a 160-bit address as argument

### Source Code

There two relevant files regarding the EOSIO EVM contract are:

- [evm.hpp](src/evm.hpp) A clean-room portable self-contained EVM interpreter
- [evm.cpp](contracts/evm/evm.cpp) The contract carrying EOSIO specifics

These files are documented with comments.

Other relevant source files:

- [evm.cpp](src/evm.cpp) This is the standalone environment used as base for tests
- [tester.py](tests/tester.py) This is the script that coordinates the execution of tests in standalone mode
- [tester-eosio.py](tests/tester-eosio.py) This is the script that coordinates the execution of tests on the local EOSIO node
- [deploy.py](tests/sol/deploy.py) This is the script helps packaging EVM bytecode into unsigned transactions to be used with the `raw` action
- [WSYS.sol](tests/sol/WSYS.sol) This is the sample ERC-20 contract that implements Wrapped SYS (WSYS)

### Changes to EOSIO software

It does not make much sense to implement the crypto primitives required by the EVM in WebAssembly as it exhibits poor performance and bloats the binary. Therefore we have introduced 7 additional intrinsics to the EOSIO contract environment:

- evm_keccak256
- evm_ecrecover
- evm_bigmodexp
- evm_bn256add
- evm_bn256scalarmul
- evm_bn256pairing
- evm_blake2f

This EOSIO/eos customization can be seen in the [eos-v2.0.3.patch](support/eos-v2.0.3.patch) file or in the respective [eos-D9ZB93LES8](https://github.com/simplensolid/eos-D9ZB93LES8/commit/f236f2791ad4c10e81ab60df6905a367dc4b253f) commit.

For simplicity, it includes an exact copy of [evm.hpp](src/evm.hpp) even though only the crypto primitives are required (the compiler throws away the unused code).

We also modified the `maximum_call_depth` in order to accommodate the EVM call stack limit of 1024. Alternativelly, the interpreter could have used a heap allocated stack instead, but naturally using the CPP stack makes the code easier to read and maintain.

The EOSIO/eosio.cdt customization is simply glue code to support the additional list of intrinsics as can be seen in the [eosio.cdt-v1.7.0.patch](support/eosio.cdt-v1.7.0.patch) file or in the [eosio.cdt-D9ZB93LES8](https://github.com/simplensolid/eosio.cdt-D9ZB93LES8/commit/a0d0dfb732ac1df3e39ed014a1eb06d3fa682f3b) commit.

### Getting Started

Clone this repository:

    $ git clone git@github.com:simplensolid/D9ZB93LES8.git
    $ cd D9ZB93LES8

Run the docker image with the modified EOSIO software stack (or follow the [Dockerfile](docker/Dockerfile) steps manually). It will take several minutes to build the modified versions of EOSIO/eos and EOSIO/eosio.cdt:

    $ docker build -t D9ZB93LES8 ./docker/
    $ docker run -it --rm -v $PWD:/home/ubuntu/D9ZB93LES8/ -w /home/ubuntu/D9ZB93LES8/ D9ZB93LES8

Compile the contract (using the modified eosio-cpp):

    $ eosio-cpp -fno-stack-first -stack-size 8388608 -DNDEBUG -DNATIVE_CRYPTO -O=z contracts/evm/evm.cpp

One can also compile the contract for the vanilla EOSIO software stack where the crypto primitives are implemented in WASM (not recommended):

    $ eosio-cpp -fno-stack-first -stack-size 2097152 -DNDEBUG -O=z contracts/evm/evm.cpp

## Running the test suite

One can run the test suite both in standalone mode and EOSIO mode. Each test needs to be compiled and is cached. The first run is always slow.

In standalone mode we test only the EVM interpreter running on native code.

    $ cd tests
    $ python3 tester.py
    $ python3 tester.py ../support/tests/VMTests/vmLogTest/
    $ python3 tester.py ../support/tests/VMTests/vmLogTest/log0_emptyMem.json

In EOSIO mode we test the EVM execution on top of the WASM based EOS-VM.

    $ cd tests
    $ python3 tester-eosio.py
    $ python3 tester-eosio.py ../support/tests/VMTests/vmLogTest/
    $ python3 tester-eosio.py ../support/tests/VMTests/vmLogTest/log0_emptyMem.json

The EOSIO tester will kill `nodeos` and wipe out `~/eosio-wallet/` and `~/.local/share/eosio/` folders at every test, check the details on the auxiliary script (start-eosio-clean.sh)[tests/start-eosio-clean.sh]. Therefore in order to run tests in parallel, one should instantiate multiple containers.

Important note:

- All tests pass in standalone mode, except a small inconsistent set (regarding gas usage) for which the implementation is consistent with the behavior of `geth`
- Some tests take longer than 350ms in EOSIO mode and fail with code 3080004 (Transaction exceeded the current CPU usage limit imposed on the transaction) or 3080006 (Transaction took too long)
- Both lists are documented in [failing.txt](tests/failing.txt)

## Testing an ERC-20 implementation

