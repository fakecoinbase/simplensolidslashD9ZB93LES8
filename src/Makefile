CPPC=g++ -std=c++11 -Os
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
