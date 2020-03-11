all: evm.wasm

clean:
	rm -f *.abi *.wasm

evm.wasm: evm.cpp ../../src/evm.hpp
	eosio-cpp -fno-stack-first -stack-size 8388608 -DNDEBUG -DNATIVE_CRYPTO -O=z -o $@ $<
