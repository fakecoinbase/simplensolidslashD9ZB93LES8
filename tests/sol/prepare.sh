#/bin/sh

SCRIPT=`realpath -s $0`
FOLDER=`dirname $SCRIPT`

# reset the eosio node
pkill nodeos
sleep 1
rm -rf ~/eosio-wallet/
rm -rf ~/.local/share/eosio/
nodeos -e -p eosio \
	--plugin eosio::producer_plugin \
	--plugin eosio::chain_api_plugin \
	--plugin eosio::http_plugin \
	--plugin eosio::history_plugin \
	--plugin eosio::history_api_plugin \
	--filter-on="*" \
	--access-control-allow-origin='*' \
	--contracts-console \
	--http-validate-host=false \
	--max-transaction-time=350 \
	--abi-serializer-max-time-ms=30 \
	--genesis-json $FOLDER/../../docker/genesis.json \
	--verbose-http-errors >> /tmp/nodeos.log 2>&1 &
sleep 1

# create the wallet and import relevant keys
cleos wallet create --to-console
echo '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3' | cleos wallet import
echo '5J2CnKzrj7XU2nHdzqrJ5yZou94tUUk3HB3TiArDCFgSAQmA6qc' | cleos wallet import

# sets up the eosio.token contract and issues SYS
cleos create account eosio eosio.token EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
cd $FOLDER/../../support/eosio.contracts/contracts/eosio.token/
eosio-cpp -I include -o eosio.token.wasm src/eosio.token.cpp --abigen
cleos set contract eosio.token . --abi eosio.token.abi -p eosio.token@active
cleos push action eosio.token create '["eosio", "1000000000.0000 SYS"]' -p eosio.token@active
cleos push action eosio.token issue '["eosio", "10000.0000 SYS", "memo"]' -p eosio@active

# sets up the evm contract
cleos create account eosio evm EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs -p eosio@active
cleos set account permission evm active --add-code
cd $FOLDER/../../contracts/evm
make
cleos set contract evm . -p evm@active

# creates a wallet for alice
cleos create account eosio alice EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
cleos push action eosio.token transfer '["eosio", "alice", "1000.0000 SYS", "memo"]' -p eosio@active
cleos push action evm create '["alice", ""]' -p alice@active
cleos push action eosio.token transfer '["alice", "evm", "1000.0000 SYS", "memo"]' -p alice@active

# creates a wallet for bob
cleos create account eosio bob EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs
cleos push action eosio.token transfer '["eosio", "bob", "1000.0000 SYS", "memo"]' -p eosio@active
cleos push action evm create '["bob", ""]' -p bob@active
cleos push action eosio.token transfer '["bob", "evm", "1000.0000 SYS", "memo"]' -p bob@active

