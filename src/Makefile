all: evm

clean:
	rm -f evm

evm: evm.cpp evm.hpp
	g++ -std=c++11 -pedantic -Wall -Wno-vla -O3 -s -o $@ $<
