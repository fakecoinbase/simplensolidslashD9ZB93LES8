#!/bin/sh

pkill nodeos
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
	--max-transaction-time=300 \
	--genesis-json ~/eosio/docker/genesis.json \
	--verbose-http-errors >> /tmp/nodeos.log 2>&1 &
sleep 1
cleos wallet create --to-console
echo '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3' | cleos wallet import
echo '5J2CnKzrj7XU2nHdzqrJ5yZou94tUUk3HB3TiArDCFgSAQmA6qc' | cleos wallet import
cleos create account eosio evm EOS72H7tRCDjsPzkdYjmdSnENCrA25D2Q1ZopCyYojBd9cjVa2yqs -p eosio@active
cleos set contract evm $1 -p evm@active

