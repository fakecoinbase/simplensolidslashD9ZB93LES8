.PHONY: all clean

.SUFFIXES: .hpp .cpp .o

all: evm

clean:
	rm -f evm *.o

evm: evm.o
	g++ -s -o evm evm.cpp

evm.o: evm.hpp evm.cpp

.cpp.o:
	g++ -std=c++11 -pedantic -Wall -Wno-vla -Wno-unused-variable -Os -o $@ $<

