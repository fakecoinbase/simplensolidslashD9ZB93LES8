SRCS=\
	evm.cpp

ABIS=$(SRCS:.cpp=.abi)
ASMS=$(SRCS:.cpp=.wasm)

.PHONY: all clean

.SUFFIXES: .cpp .abi .wasm

all: $(ABIS) $(ASMS)

clean:
	rm -f $(ABIS) $(ASMS)

.cpp.abi:
	eosio-cpp $< -o $@

.cpp.wasm:
	eosio-cpp $< -o $@
