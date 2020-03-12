## EOSIO Challenge Submission

_This documentation is concise on purpose. Please get in touch should you have any questions._

The submission implements all the functionality featured in the Technical Requirements and passes all relevant tests in the test suite.
To our best knowledge it is complete and compatible with Geth.
**If after evaluation it does not satisfy any judging criteria, a short report describing what needs to be fixed is very much appreciated.**

1. [Submission Notes](#notes)
2. [Source Code](#code)
3. [Changes to EOSIO software](#changes)
4. [Getting started](#start)
5. [Compiling the contract](#compile)
6. [Running the test suite](#test)
7. [Testing an ERC-20 implementation](#erc20)

### 1. Submission Notes<a name="notes"></a>

- The code implements an EVM interpreter, it is self-contained, supports all opcodes and all 9 precompiled contracts, and does not rely on additional libraries
- We have modified EOSIO/eos and EOSIO/eosio.cdt to support keccak256, and other cryptographic primitives, as EOSIO intrinsics
- We have increased EOSIO/eos `maximum_call_depth` in order to accommodate the EVM call stack limit of 1024
- The current curve BN256 code is not optimized for production and could run significantly faster otherwise

Regarding the technical requirements:

- Current block number returned by `NUMBER` is the one provided by `eosio::tapos_block_num()`, as no other alternative was found in the EOSIO platform documentation
- The Chain ID is hardcoded but can be modified by editting the `CHAIN_ID` constant of the interpreter [[evm.hpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/src/evm.hpp#L2766)]
- The implementation may support any past release of Ethereum based on the `forknumber()` method, implemented by the contract [[evm.cpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/contracts/evm/evm.cpp#L591)], and the `releaseforkblock` table, hardcoded into the interpreter [[evm.hpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/src/evm.hpp#L2784)]
- The current version of the `raw` action always check if the sender address has an EOSIO account associated with it (as understood from the requirements). To remove this constraint for signed transactions, please comment line 448 of [[evm.cpp](https://github.com/simplensolid/D9ZB93LES8/blob/741b261ea8f91e3675956688a9920f884ad69ad8/contracts/evm/evm.cpp#L448)]
- For testing purposes, one can easily query any EVM account for its contents using the `inspect` action which takes an 160-bit address as argument

Regarding EOSIO transaction time/cpu usage:

- EOSIO imposes limits to transaction time/cpu usage, but the Technical Requirements fail to specify the desired behavior for when these parameters are exceeded. Therefore, we did not conceive any mechanism to overcome these limits in order to complete EVM transactions that might exceed them (besides working on the interpreter performance)
- We have tested our implementation with a 350ms limit, which naturally rules out some valid EVM transactions that take too long to complete
- Most tests in the test suite execute under 150ms

### 2. Source Code<a name="code"></a>

There are two relevant files regarding the EOSIO EVM contract:

- [src/evm.hpp](src/evm.hpp) A clean-room portable self-contained EVM interpreter
- [contracts/evm/evm.cpp](contracts/evm/evm.cpp) The contract carrying EOSIO specifics

These files are fairly readable and documented with comments. We encourage reading them.

Other relevant source files:

- [src/evm.cpp](src/evm.cpp) This is the standalone environment used as base for tests
- [tests/tester.py](tests/tester.py) This is the script that coordinates the execution of tests in standalone mode
- [tests/tester-eosio.py](tests/tester-eosio.py) This is the script that coordinates the execution of tests on the local EOSIO node
- [tests/sol/deploy.py](tests/sol/deploy.py) This is the script that helps packaging EVM bytecode into unsigned transactions to be used with the `raw` action
- [tests/sol/WSYS.sol](tests/sol/WSYS.sol) This is the sample ERC-20 contract that implements Wrapped-SYS (WSYS)

### 3. Changes to EOSIO software<a name="changes"></a>

It does not make much sense to implement the crypto primitives required by the EVM in WebAssembly as it exhibits poor performance and bloats the WASM binary. Therefore we have introduced 7 additional intrinsics to the EOSIO contract environment:

- evm_keccak256
- evm_ecrecover
- evm_bigmodexp
- evm_bn256add
- evm_bn256scalarmul
- evm_bn256pairing
- evm_blake2f

This EOSIO/eos customization can be seen in the [eos-v2.0.3.patch](support/eos-v2.0.3.patch) file or in the respective [eos-D9ZB93LES8](https://github.com/simplensolid/eos-D9ZB93LES8/commit/f236f2791ad4c10e81ab60df6905a367dc4b253f) commit.

For simplicity, it includes an exact copy of [evm.hpp](src/evm.hpp), even though only the crypto primitives are required (the compiler throws away the unused code).

We also modified the `maximum_call_depth` in order to accommodate the EVM call stack limit of 1024. Alternativelly, the interpreter could have used a heap allocated stack instead, but naturally using the C++ call stack as the interpreter call stack makes the code easier to read and maintain.

The EOSIO/eosio.cdt customization is simply glue code to support the additional list of intrinsics as can be seen in the [eosio.cdt-v1.7.0.patch](support/eosio.cdt-v1.7.0.patch) file or in the [eosio.cdt-D9ZB93LES8](https://github.com/simplensolid/eosio.cdt-D9ZB93LES8/commit/a0d0dfb732ac1df3e39ed014a1eb06d3fa682f3b) commit.

### 4. Getting started<a name="start"></a>

Clone this repository:

    $ git clone --recursive git@github.com:simplensolid/D9ZB93LES8.git
    $ cd D9ZB93LES8

Run the Docker image with the modified EOSIO software stack (or follow the [Dockerfile](docker/Dockerfile) steps manually). It will take several minutes to build the modified versions of EOSIO/eos and EOSIO/eosio.cdt:

    $ docker build -t custom-eosio ./docker/
    $ docker run -it --rm -v $PWD:/home/ubuntu/D9ZB93LES8/ -w /home/ubuntu/D9ZB93LES8/ custom-eosio

To run the tests (not using the [Dockerfile](docker/Dockerfile)), it is necessary to install the dependencies.

For Python

    $ pip3 install ecdsa pyyaml --user

For Node

    $ npm i -g solc@0.6.3

### 5. Compiling the contract<a name="compile"></a>

Compile the contract (using the modified eosio-cpp):

    $ eosio-cpp -fno-stack-first -stack-size 8388608 -DNDEBUG -DNATIVE_CRYPTO -O=z contracts/evm/evm.cpp

One can also compile the contract for the vanilla EOSIO software stack where the crypto primitives are implemented in WASM (not recommended):

    $ eosio-cpp -fno-stack-first -stack-size 2097152 -DNDEBUG -O=z contracts/evm/evm.cpp

### 6. Running the test suite<a name="test"></a>

One can run the test suite both in standalone mode and EOSIO mode. Each test needs to be compiled and is cached, therefore the first run is always slow. Tests can be filtered using their path.

In standalone mode, we test only the EVM interpreter running nativelly (compiled with g++)

    $ cd tests
    $ python3 tester.py
    $ python3 tester.py ../support/tests/VMTests/vmLogTest/
    $ python3 tester.py ../support/tests/VMTests/vmLogTest/log0_emptyMem.json

In EOSIO mode, we test the EVM execution on top of the WASM based EOS-VM

    $ cd tests
    $ python3 tester-eosio.py
    $ python3 tester-eosio.py ../support/tests/VMTests/vmLogTest/
    $ python3 tester-eosio.py ../support/tests/VMTests/vmLogTest/log0_emptyMem.json

The EOSIO tester will kill `nodeos` and wipe out `~/eosio-wallet/` and `~/.local/share/eosio/` folders at every test (check the details on the auxiliary script [start-eosio-clean.sh](tests/start-eosio-clean.sh)). Therefore in order to run tests in parallel, one should instantiate multiple Docker containers.

Important note:

- All tests pass in standalone mode, except a small inconsistent (regarding gas usage) set for which the implementation follows the behavior of Geth
- Some tests take longer than 350ms in EOSIO mode and fail with code `3080004` (Transaction exceeded the current CPU usage limit imposed on the transaction), `3080006` (Transaction took too long), or `3040005` (Expired Transaction)
- These test lists are documented in [failing.txt](tests/failing.txt)

### 7. Testing an ERC-20 implementation<a name="erc20"></a>

In order to test an ERC-20 implementation we need to prepare the EOSIO enviroment. Please run the [prepare.sh](tests/sol/prepare.sh) script or follow the steps manually.

    $ cd tests/sol
    $ ./prepare.sh

The preparation steps create two accounts, one for Alice and one for Bob, and issue them 1000.0000 SYS each. In this test we will make Bob send Alice 100.0000 SYS via the Wrapped-SYS (WSYS) ERC-20 token.

First we need to compile and deploy the [WSYS](tests/sol/WSYS.sol) ERC-20 contract (using a third account):

    $ solcjs --bin WSYS.sol
    $ cleos push action evm create '["evm", ""]' -p evm@active
    $ python deploy.py create 1 WSYS_sol_WSYS.bin
    $ cleos push action evm raw '["f91027...", "13729111c844844d28e2c6162bc57aa78dd97bdd"]' -p evm@active
    $ python deploy.py address 13729111c844844d28e2c6162bc57aa78dd97bdd 1
    $ cleos push action evm inspect '["b44e6ef3336e9ff9c30290000214ca394086154a"]' -p evm@active

Then Bob deposits 100.0000 SYS into the contract

    $ cleos get table eosio.token bob accounts
    $ cleos push action evm create '["bob", ""]' -p bob@active
    $ cleos push action eosio.token transfer '["bob", "evm", "100.0000 SYS", "memo"]' -p bob@active
    $ cleos get table eosio.token evm accounts
    $ cleos get table eosio.token bob accounts
    $ cleos push action evm inspect '["92eac575633155df1667fc4cd67f855d2228d591"]' -p bob@active

wraps that amount in WSYS

    $ python deploy.py wrap 1 b44e6ef3336e9ff9c30290000214ca394086154a 1000000
    $ cleos push action evm raw '["e70101...", "92eac575633155df1667fc4cd67f855d2228d591"]' -p bob@active
    $ cleos push action evm inspect '["92eac575633155df1667fc4cd67f855d2228d591"]' -p bob@active
    $ cleos push action evm inspect '["b44e6ef3336e9ff9c30290000214ca394086154a"]' -p bob@active

and transfer 100.0000 WSYS to Alice

    $ python deploy.py transfer 2 b44e6ef3336e9ff9c30290000214ca394086154a fde9818b4bf62c6507efb33ddcec5705eed74325 1000000
    $ cleos push action evm raw '["f86502...", "92eac575633155df1667fc4cd67f855d2228d591"]' -p bob@active
    $ cleos push action evm inspect '["b44e6ef3336e9ff9c30290000214ca394086154a"]' -p bob@active

Finally, Alice unwraps the WSYS

    $ cleos get table eosio.token alice accounts
    $ cleos push action evm create '["alice", ""]' -p alice@active
    $ cleos push action evm inspect '["fde9818b4bf62c6507efb33ddcec5705eed74325"]' -p alice@active
    $ python deploy.py unwrap 1 b44e6ef3336e9ff9c30290000214ca394086154a 1000000
    $ cleos push action evm raw '["f84401...", "fde9818b4bf62c6507efb33ddcec5705eed74325"]' -p alice@active
    $ cleos push action evm inspect '["b44e6ef3336e9ff9c30290000214ca394086154a"]' -p alice@active
    $ cleos push action evm inspect '["fde9818b4bf62c6507efb33ddcec5705eed74325"]' -p alice@active

and withdraws 100.0000 SYS

    $ cleos push action evm withdraw '["alice", "100.0000 SYS"]' -p alice@active
    $ cleos get table eosio.token alice accounts
    $ cleos get table eosio.token evm accounts
    $ cleos push action evm inspect '["fde9818b4bf62c6507efb33ddcec5705eed74325"]' -p alice@active

You can watch a step-by-step video on [YouTube](https://youtu.be/Wsq4jQ61-Us).

