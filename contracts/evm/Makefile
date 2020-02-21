all: evm.wasm

clean:
	rm -f *.abi *.wasm

evm.wasm: evm.cpp ../../src/evm.hpp
	eosio-cpp -DNDEBUG -o $@ $<
