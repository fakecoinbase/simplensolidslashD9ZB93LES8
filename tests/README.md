## Configuration

Bellow are the steps required to setup and reproduce the environment for this submission.

The whole setup can be skiped by using the provided docker configuration

    $ docker/start.sh

However we do replicate the steps in this document in case docker is not suitable.

Note that for all examples we assume Ubuntu 18.04 is the underlying platform.

Use the following command to get a Ubuntu 18.04 docker running in the project folder:

    $ docker run -it -v $PWD/:/home/eosio/ -w /home/eosio/ ubuntu:18.04

### Setting up the EOSIO environment

There are the steps to get the EOSIO blockchain and environment running.

We have modified two parameters from the default configuration. Both max-transaction-time and max_transaction_cpu_usage
were increased by a factor of 10. Since we are running a interpreter 10x seems a reasonable measure for the performance penalty.

    $ apt-get update
    $ apt-get install -y libcurl3-gnutls libtinfo5 libusb-1.0-0 wget
    $ wget http://security.ubuntu.com/ubuntu/pool/main/i/icu/libicu60_60.2-3ubuntu3_amd64.deb
    $ wget https://github.com/EOSIO/eos/releases/download/v2.0.3/eosio_2.0.3-1-ubuntu-18.04_amd64.deb
    $ wget https://github.com/EOSIO/eosio.cdt/releases/download/v1.7.0/eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb
    $ dpkg -i libicu60_60.2-3ubuntu3_amd64.deb
    $ dpkg -i eosio_2.0.3-1-ubuntu-18.04_amd64.deb
    $ dpkg -i eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb
    $ sudo keosd &
    $ nodeos -e -p eosio \
        -d \
        --plugin eosio::producer_plugin \
        --plugin eosio::chain_api_plugin \
        --plugin eosio::http_plugin \
        --plugin eosio::history_plugin \
        --plugin eosio::history_api_plugin \
        --filter-on="*" \
        --access-control-allow-origin='*' \
        --contracts-console \
        --http-validate-host=false \
        --max-transaction-time=1500000 \
        --genesis-json ./docker/genesis.json \
        --verbose-http-errors >> /tmp/nodeos.log 2>&1 &

### Setting up the EOSIO wallet and eosio.token contract

There are the steps to configure the SYS token in the local EOS blockchain:

    $ cleos wallet create --to-console
    $ echo '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3' | cleos wallet import
    $ echo '5J2CnKzrj7XU2nHdzqrJ5yZou94tUUk3HB3TiArDCFgSAQmA6qc' | cleos wallet import
    $ cd support/eosio.contracts/contracts/eosio.token/
    $ eosio-cpp -I include -o eosio.token.wasm src/eosio.token.cpp --abigen
    $ cleos create account eosio eosio.token EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
    $ cleos set contract eosio.token . --abi eosio.token.abi -p eosio.token@active
    $ cleos push action eosio.token create '["eosio", "1000000000.0000 SYS"]' -p eosio.token@active
    $ cleos push action eosio.token issue '["eosio", "10000.0000 SYS", "memo"]' -p eosio@active

### Setting up the EVM contract

These are the steps to get the EVM contract up and running:

    $ cd contracts/evm
    $ eosio-cpp -DNDEBUG -O=z -o evm.wasm evm.cpp
    $ cleos create account eosio evm EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs -p eosio@active
    $ cleos set account permission evm active --add-code
    $ cleos set contract evm . -p evm@active

## Creating test user wallets and EVM accounts

There are the steps to setup two test accounts, one for Alice and anoter for Bob:

    $ cleos create account eosio alice EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
    $ cleos push action eosio.token transfer '["eosio", "alice", "1000.0000 SYS", "memo"]' -p eosio@active
    $ cleos push action evm create '["alice", ""]' -p alice@active
    $ cleos push action eosio.token transfer '["alice", "evm", "1000.0000 SYS", "memo"]' -p alice@active

    $ cleos create account eosio bob EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
    $ cleos push action eosio.token transfer '["eosio", "bob", "1000.0000 SYS", "memo"]' -p eosio@active
    $ cleos push action evm create '["bob", ""]' -p bob@active
    $ cleos push action eosio.token transfer '["bob", "evm", "1000.0000 SYS", "memo"]' -p bob@active

Each account receives 1000 SYS which are then deposited into the EVM contract after their accounts are created.

### Compiling the ERC-20 contract

We tested the EOSIO contract using an ERC-20 implementation of WSYS, a Wrapper SYS token.
The ERC-20 token source code provides two adittional methods wrap and unwrap that can be used to convert SYS into WSYS and vice-versa.

Here are the steps to compile the ERC-20 token with the solidity compiler:

    $ apt-get update
    $ apt-get install npm
    $ npm i -g npx solc
    $ cd tests/sol
    $ npx solc --bin WSYS.sol

It produces a WSYS.bin file that can then wrapped in a transaction and sent to the contract.

### Executing the ERC-20 contract

### Running the test suite

This software was tested using the test suite provided. A great effort was put to get every single test through.

The implementation tested was a standalone version generated and compiled for each test (the files are stored in the tests/cache folder).

Therefore it tests the interpreter, and not the actual contract.

Following is the setup steps to execute the test suite.

    $ apt-get update
    $ apt-get install -y g++ python3 python3-pip
    $ pip3 install ecdsa pyyaml
    $ cd tests
    $ python3 tester.py

The time it takes to run all tests is long, as each test is compiled before executes. A second run goes faster due to caching.

To run each test or test folder individually, simply provide the path as argument, for example:

    $ python3 tester.py ../support/tests/VMTests/

    $ python3 tester.py ../support/tests/GeneralStateTests/

    $ python3 tester.py ../support/tests/TransactionTests/

