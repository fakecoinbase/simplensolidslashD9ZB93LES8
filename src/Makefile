CPPC=g++ -std=c++11 -pedantic -Wall -Wno-vla -Wno-unused-variable -Wno-unused-but-set-variable -Os
CPPFLAGS=

CL=g++ -s
CLFLAGS=

EXE=evm

SRCS=\
	evm.cpp

OBJS=$(SRCS:.cpp=.o)

.PHONY: all clean

.SUFFIXES: .cpp .o

all: $(EXE)

clean:
	rm -f $(EXE) $(OBJS)

$(EXE): $(OBJS)
	$(CL) $(OBJS) $(CLFLAGS) -o $(EXE)

.cpp.o:
	$(CPPC) -c $(CPPFLAGS) -o $@ $<
