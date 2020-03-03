all: WSYS.bin

clean:
	rm -f *.abi *.bin

WSYS.bin: WSYS.sol
	npx solc --bin WSYS.sol
	mv WSYS_sol_WSYS.bin WSYS.bin
	rm -f WSYS_sol_*.bin
