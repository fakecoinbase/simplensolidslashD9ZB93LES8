all: WSYS.bin

clean:
	rm -f *.abi *.bin

WSYS.bin: WSYS.sol
	solcjs --bin WSYS.sol
	mv WSYS_sol_WSYS.bin WSYS.bin
	rm -f WSYS_sol_*.bin
