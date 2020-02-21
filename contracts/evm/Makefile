all: evm.wasm

clean:
	rm -f *.abi *.wasm

evm.wasm: evm.cpp evm.hpp ../../src/evm.hpp
	eosio-cpp -DNDEBUG -o $@ $<
