.PHONY: all clean

.SUFFIXES: .hpp .cpp .abi .wasm

all: evm.abi evm.wasm

clean:
	rm -f *.abi *.wasm

evm.o: evm.hpp evm.cpp

.cpp.abi:
	eosio-cpp -DNDEBUG -o $@ $<

.cpp.wasm:
	eosio-cpp -DNDEBUG -o $@ $<
