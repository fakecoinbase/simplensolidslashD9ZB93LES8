all: evm

clean:
	rm -f evm

evm: evm.cpp evm.hpp
	g++ -std=c++11 -pedantic -Wall -O3 -s -o $@ $<
