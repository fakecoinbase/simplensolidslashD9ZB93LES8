#!/bin/sh

SCRIPT=`realpath -s $0`
FOLDER=`dirname $SCRIPT`

# attempts to prepare the environment and set the test contract in a robust way
while true
do
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
		--genesis-json $FOLDER/../docker/genesis.json \
		--verbose-http-errors >> /tmp/nodeos.log 2>&1 &
	sleep 1
	cleos wallet create --to-console
	echo '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3' | cleos wallet import
	echo '5J2CnKzrj7XU2nHdzqrJ5yZou94tUUk3HB3TiArDCFgSAQmA6qc' | cleos wallet import
	if cleos create account eosio evm EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs -p eosio@active; then
		break
	fi
	sleep 1
	if cleos create account eosio evm EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs -p eosio@active; then
		break
	fi
done
if ! cleos set contract evm $1 -p evm@active; then
	sleep 3
	if ! cleos set contract evm $1 -p evm@active; then
		sleep 5
		cleos set contract evm $1 -p evm@active
	fi
fi

