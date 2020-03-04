# EOSIO Challenge Submission

This is a "to-the-point" documentation. Please get in touch should you have any questions.

## Release Notes

- To my best knowloedge the code is fully functional
- As with any first release of software, there might exist bugs, however they can be fixed with little effort
- Implements compatibility multiple releases
- Compatible with geth
- Self contained, all code was written, no 3rd party library used
- Everything my IP
- Lessa than xKb
- I value readability and code reuse, usually not compatible with hackathon spirit

## Known issues

- Slow EXPMOD

## Considerations

- The spec was not fully specific, so I assumed that by first submission
was subjective in the sense that a more complete submission made later
could disqualify a less complete submited later, therefore I decided to
spend a little more time on delivering a more complete version, despite
the fact that it could take more time

- I also assume that for the prize of the challenge EOSIO would also consider
important the quality, therefore I put an effort to provide a more readable and
reusable code

## Configuration

    $ docker/start.sh

    $ docker run -it -v $PWD/:/home/eosio/ -w /home/eosio/ ubuntu:18.04

### Setting up the EOSIO environment

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

    $ cd contracts/evm
    $ eosio-cpp -DNDEBUG -O=z -o evm.wasm evm.cpp
    $ cleos create account eosio evm EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs -p eosio@active
    $ cleos set account permission evm active --add-code
    $ cleos set contract evm . -p evm@active

## Creating test user wallets and EVM accounts

    $ cleos create account eosio alice EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
    $ cleos push action eosio.token transfer '["eosio", "alice", "1000.0000 SYS", "memo"]' -p eosio@active
    $ cleos push action evm create '["alice", ""]' -p alice@active
    $ cleos push action eosio.token transfer '["alice", "evm", "1000.0000 SYS", "memo"]' -p alice@active

    $ cleos create account eosio bob EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
    $ cleos push action eosio.token transfer '["eosio", "bob", "1000.0000 SYS", "memo"]' -p eosio@active
    $ cleos push action evm create '["bob", ""]' -p bob@active
    $ cleos push action eosio.token transfer '["bob", "evm", "1000.0000 SYS", "memo"]' -p bob@active

### Compiling the ERC-20 contract

    $ apt-get update
    $ apt-get install npm
    $ npm i -g npx solc
    $ cd tests/sol
    $ npx solc --bin WSYS.sol

### Running the test suite

    $ apt-get update
    $ apt-get install -y g++ python3 python3-pip
    $ pip3 install ecdsa pyyaml
    $ cd tests
    $ python3 tester.py
    $ python3 tester.py ../support/tests/VMTests/
    $ python3 tester.py ../support/tests/GeneralStateTests/
    $ python3 tester.py ../support/tests/TransactionTests/

