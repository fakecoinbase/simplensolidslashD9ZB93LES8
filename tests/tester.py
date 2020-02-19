import json, os, subprocess, sys

def _report(*msg):
    print("== " + sys.argv[0] + ":", *msg, file=sys.stderr)

def _die(*msg, exit_code=1):
    _report(*msg)
    _report("exiting...")
    sys.exit(exit_code)

def findTests(baseDir=".", filePrefix=""):
    return [ fullTest for fullTest in [ os.path.join(root, file) for root, _, files in os.walk(baseDir)
                                                                 for file in files
                                                                  if file.endswith(".json") or file.endswith(".yml")
                                      ]
                       if fullTest.startswith(filePrefix)
           ]

def listTests(baseDir=".", filePrefixes=[""]):
    return [ test for fPrefix in filePrefixes
                  for test in findTests(baseDir=baseDir, filePrefix=fPrefix)
           ]

def readFile(fname):
    if not os.path.isfile(fname):
        _die("Not a file:", fname)
    with open(fname, "r") as f:
        fcontents = f.read()
        try:
            if fname.endswith(".json"):
                fparsed = json.loads(fcontents)
            elif fname.endswith(".yml"):
                fparsed = yaml.load(fcontents, Loader=yaml.FullLoader)
            elif fname.endswith(".cpp"):
                fparsed, _ = fcontents.split("/* main */")
            elif fname.endswith(".hpp"):
                fparsed = fcontents
            else:
                _die("Do not know how to load:", fname)
            return fparsed
        except:
            _die("Could not load file:", fname)

def writeFile(fname, fcontents):
    if not os.path.exists(os.path.dirname(fname)):
        os.makedirs(os.path.dirname(fname))
    with open(fname, "w") as f:
        f.write(fcontents)

def compileFile(fnamein, fnameout):
    if os.path.isfile(fnameout): return;
    return subprocess.call([
        "g++",
        "-std=c++14",
        "-pedantic",
        "-Wall",
        "-Wno-vla",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Os",
        "-o",
        fnameout,
        fnamein,
    ])

def execFile(fnamein):
    return subprocess.call([fnamein])

def hexToInt(s):
    if s[:2] == "0x": s = s[2:]
    if s == "": s = "0"
    return int(s, 16)

def hexToBin(s):
    if s[:2] != "0x": _die("Not hex:", s)
    s = s[2:]
    hexv = ""
    hexs = ("0" if len(s) % 2 != 0 else "") + s
    for i in range(0, len(s) // 2):
        hexv += "\\x" + hexs[:2]
        hexs = hexs[2:]
    return hexv

def intToU256(v):
    if v < 0 or v > 2**256-1: _die("Not U256:", v)
    hexv = ""
    hexs = "%064x" % v
    for i in range(0, 32):
        hexv += "\\x" + hexs[:2]
        hexs = hexs[2:]
    return hexv    

def codeInitExec(origin, gasprice, address, caller, value, gas, code, data):
    return """
    uint160_t origin = (uint160_t)uint256_t::from(\"""" + intToU256(origin) + """\");
    uint256_t gasprice = uint256_t::from(\"""" + intToU256(gasprice) + """\");
    uint160_t address = (uint160_t)uint256_t::from(\"""" + intToU256(address) + """\");
    uint160_t caller = (uint160_t)uint256_t::from(\"""" + intToU256(caller) + """\");
    uint256_t value = uint256_t::from(\"""" + intToU256(value) + """\");
    uint256_t _gas = uint256_t::from(\"""" + intToU256(gas) + """\");

    uint64_t gas = _gas.cast64(); // fix

    uint8_t *code = (uint8_t*)\"""" + code + """\";
    uint64_t code_size = """ + str(len(code) // 4) + """;
    uint8_t *data = (uint8_t*)\"""" + data + """\";
    uint64_t data_size = """ + str(len(data) // 4) + """;
"""

def codeInitEnv(timestamp, number, coinbase, gaslimit, difficulty):
    return """
    uint64_t timestamp = uint256_t::from(\"""" + intToU256(timestamp) + """\").cast64();
    uint256_t number = uint256_t::from(\"""" + intToU256(number) + """\");
    uint160_t coinbase = (uint160_t)uint256_t::from(\"""" + intToU256(coinbase) + """\");
    uint64_t gaslimit = uint256_t::from(\"""" + intToU256(gaslimit) + """\").cast64();
    uint256_t difficulty = uint256_t::from(\"""" + intToU256(difficulty) + """\");
"""

def codeInitLocation(location, number):
    return """
        {
            uint256_t location = uint256_t::from(\"""" + intToU256(location) + """\");
            uint256_t number = uint256_t::from(\"""" + intToU256(number) + """\");
            state.store(account, location, number);
        }
"""

def codeInitRlp(rlp):
    return """
    uint8_t *buffer = (uint8_t*)\"""" + rlp + """\";
    uint64_t size = """ + str(len(rlp) // 4) + """;
"""

def codeDoneLocation(location, number):
    return """
        {
            uint256_t location = uint256_t::from(\"""" + intToU256(location) + """\");
            uint256_t number = uint256_t::from(\"""" + intToU256(number) + """\");
            uint256_t _number = state.load(account, location);
            if (number != _number) {
                std::cerr << "post: invalid storage" << std::endl;
//                std::cerr << number << std::endl;
//                std::cerr << _number << std::endl;
                return 1;
            }
        }
"""

def codeInitAccount(account, nonce, balance, code, storage):
    src = """
    {
        uint160_t account = (uint160_t)uint256_t::from(\"""" + intToU256(account) + """\");
        uint64_t nonce = uint256_t::from(\"""" + intToU256(nonce) + """\").cast64();
        uint256_t balance = uint256_t::from(\"""" + intToU256(balance) + """\");
        uint8_t *code = (uint8_t*)\"""" + code + """\";
        uint64_t code_size = """ + str(len(code) // 4) + """;
        uint256_t codehash = sha3(code, code_size);

        state.store_code(codehash, code, code_size);
        state.set_nonce(account, nonce);
        state.set_balance(account, balance);
        state.set_codehash(account, codehash);
"""
    for subkey, subvalue in storage.items():
        location = hexToInt(subkey)
        number = hexToInt(subvalue)
        src += codeInitLocation(location, number)
    src += """
    }
"""
    return src

def codeDoneAccount(account, nonce, balance, code, storage):
    src = """
    {
        uint160_t account = (uint160_t)uint256_t::from(\"""" + intToU256(account) + """\");
        uint256_t nonce = uint256_t::from(\"""" + intToU256(nonce) + """\");
        uint256_t balance = uint256_t::from(\"""" + intToU256(balance) + """\");
        uint8_t *code = (uint8_t*)\"""" + code + """\";
        uint64_t code_size = """ + str(len(code) // 4) + """;

        if (nonce != state.get_nonce(account)) {
            std::cerr << "post: invalid nonce" << std::endl;
//            std::cerr << nonce << " " << state.get_nonce(account) << std::endl;
            return 1;
        }
        if (balance != state.get_balance(account)) {
            std::cerr << "post: invalid balance" << std::endl;
//            std::cerr << balance << " " << state.get_balance(account) << std::endl;
            return 1;
        }

        uint256_t codehash = state.get_codehash(account);
        uint64_t _code_size;
        const uint8_t *_code = state.load_code(codehash, _code_size);
        if (code_size != _code_size) {
            std::cerr << "post: invalid account code_size" << std::endl;
            return 1;
        }
        for (uint64_t i = 0; i < _code_size; i++) {
            if (code[i] != _code[i]) {
                std::cerr << "post: invalid account code" << std::endl;
                return 1;
            }
        }
"""
    for subkey, subvalue in storage.items():
        location = hexToInt(subkey)
        number = hexToInt(subvalue)
        src += codeDoneLocation(location, number)
    src += """
    }
"""
    return src

def codeDoneReturn(out):
    return """
    uint8_t *out_data = (uint8_t*)\"""" + out + """\";
    uint64_t out_size = """ + str(len(out) // 4) + """;
    if (out_size != return_size) {
        std::cerr << "post: invalid out_size" << std::endl;
        for (uint64_t i = 0; i < out_size; i++) std::cerr << " " << (int)out_data[i];
        std::cerr << std::endl;
        for (uint64_t i = 0; i < return_size; i++) std::cerr << " " << (int)return_data[i];
        std::cerr << std::endl;
        return 1;
    }
    for (uint64_t i = 0; i < out_size; i++) {
        if (out_data[i] != return_data[i]) {
            std::cerr << "post: invalid out_data" << std::endl;
            for (uint64_t i = 0; i < out_size; i++) std::cerr << " " << (int)out_data[i];
            std::cerr << std::endl;
            for (uint64_t i = 0; i < out_size; i++) std::cerr << " " << (int)return_data[i];
            std::cerr << std::endl;
            return 1;
        }
    }
"""

def codeDoneLogs(loghash):
    return """
    uint256_t loghash = uint256_t::from(\"""" + intToU256(loghash) + """\");
"""

def codeDoneGas(fgas):
    return """
    uint256_t fgas = uint256_t::from(\"""" + intToU256(fgas) + """\");
    if (gas != fgas) {
//        std::cerr << "post: invalid gas " << fgas << " " << gas << std::endl;
//        return 1;
    }
"""

def codeDoneRlp(sender, _hash):
    return """
    uint256_t _hash = uint256_t::from(\"""" + intToU256(_hash) + """\");
    if (h != _hash) {
        std::cerr << "post: invalid hash " << _hash << std::endl;
        return 1;
    }

    uint160_t sender = (uint160_t)uint256_t::from(\"""" + intToU256(sender) + """\");
    if (sender != sender) {
        std::cerr << "post: invalid sender " << " " << sender << std::endl;
        return 1;
    }
"""

def vmTest(name, item, path):
    src = readFile("../src/evm.cpp")
    hdr = readFile("../src/evm.hpp")
    src = src.replace("#include \"evm.hpp\"", hdr)

    src += """
int main()
{
"""

    env = item["env"]
    timestamp = hexToInt(env["currentTimestamp"])
    number = hexToInt(env["currentNumber"])
    coinbase = hexToInt(env["currentCoinbase"])
    gaslimit = hexToInt(env["currentGasLimit"])
    difficulty = hexToInt(env["currentDifficulty"])
    src += codeInitEnv(timestamp, number, coinbase, gaslimit, difficulty)

    src += """
    _Block block(timestamp, number, coinbase, gaslimit, difficulty);
    _State state;
    Storage storage(&state);

    Release release = get_release(block.forknumber().cast64());
"""

    exec = item["exec"]
    origin = hexToInt(exec["origin"])
    gasprice = hexToInt(exec["gasPrice"])
    gas = hexToInt(exec["gas"])
    address = hexToInt(exec["address"])
    caller = hexToInt(exec["caller"])
    value = hexToInt(exec["value"])
    code = hexToBin(exec['code'])
    data = hexToBin(exec['data'])
    src += codeInitExec(origin, gasprice, address, caller, value, gas, code, data)

    pre = item["pre"]
    for key, values in pre.items():
        account = hexToInt(key)
        nonce = hexToInt(values["nonce"])
        balance = hexToInt(values["balance"])
        code = hexToBin(values['code']);
        storage = values["storage"];
        src += codeInitAccount(account, nonce, balance, code, storage)

    src += """
    uint64_t snapshot = storage.begin();
    bool success;
    uint64_t return_size = 0;
    uint64_t return_capacity = 0;
    uint8_t *return_data = nullptr;
    _try({
        success = _catches(vm_run)(release, block, storage,
                        origin, gasprice,
                        address, code, code_size,
                        caller, value, data, data_size,
                        return_data, return_size, return_capacity, gas,
                        false, 0);
        if (std::getenv("EVM_DEBUG")) std::cerr << "vm success " << std::endl;
    }, Error e, {
        success = false;
        gas = 0;
        if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
    })
    storage.end(snapshot, success);
    uint64_t refund_gas = storage.get_refund();
    uint64_t used_gas = gaslimit - gas;
//    _refund_gas(gas, _min(refund_gas, used_gas / 2));
//    storage.add_balance(origin, gas * gasprice);
    storage.flush();
"""

    if not "out" in item:

        src += """
    if (success) {
        std::cerr << "post: invalid success on failure" << std::endl;
        return 1;
    }
"""

    else:

        src += """
    if (!success) {
        std::cerr << "post: invalid failure on success" << std::endl;
        return 1;
    }
"""
        out = hexToBin(item["out"])
        src += codeDoneReturn(out)

        post = item["post"]
        for key, values in post.items():
            account = hexToInt(key)
            nonce = hexToInt(values["nonce"])
            balance = hexToInt(values["balance"])
            code = hexToBin(values['code'])
            storage = values["storage"]
            src += codeDoneAccount(account, nonce, balance, code, storage)

        loghash = hexToInt(item["logs"])
        src += codeDoneLogs(loghash);

        fgas = hexToInt(item["gas"])
        src += codeDoneGas(fgas)

    src += """
    return 0;
}
"""

    writeFile("/tmp/" + name + ".cpp", src)
    compileFile("/tmp/" + name + ".cpp", "/tmp/" + name);
    result = execFile("/tmp/" + name)
    if result != 0:
        _report("Test failure")

def txTest(name, item, path):
    src = readFile("../src/evm.cpp")

    src += """
int main()
{
    Release release = ISTANBUL;
    _Block block;
"""

    rlp = hexToBin(item["rlp"])
    src += codeInitRlp(rlp);

    src += """
    uint256_t h;
    uint256_t from;

    bool success;
    try {
        struct txn txn = decode_txn(buffer, size);
        if (!txn.is_signed) throw INVALID_TRANSACTION;

        uint256_t offset = 8 + 2 * block.chainid();
        if (txn.v != 27 && txn.v != 28 && txn.v != 27 + offset && txn.v != 28 + offset) throw INVALID_TRANSACTION;

        {
            uint256_t v = txn.v;
            uint256_t r = txn.r;
            uint256_t s = txn.s;
            txn.is_signed = v > 28;
            txn.v = block.chainid();
            txn.r = 0;
            txn.s = 0;
            uint64_t unsigned_size = encode_txn(txn);
            uint8_t unsigned_buffer[unsigned_size];
            encode_txn(txn, unsigned_buffer, unsigned_size);
            h = sha3(unsigned_buffer, unsigned_size);
            txn.is_signed = true;
            txn.v = v > 28 ? v - offset : v;
            txn.r = r;
            txn.s = s;
        }
        from = ecrecover(h, txn.v, txn.r, txn.s);

        success = true;
        if (std::getenv("EVM_DEBUG")) std::cerr << "tx success " << std::endl;
    } catch (Error e) {
        success = false;
        if (std::getenv("EVM_DEBUG")) std::cerr << "tx exception " << errors[e] << std::endl;
    }
"""

    data = item["Istanbul"];
    if not "sender" in data:

        src += """
    if (success) {
        std::cerr << "post: invalid success on failure" << std::endl;
        return 1;
    }
"""

    else:

        src += """
    if (!success) {
        std::cerr << "post: invalid failure on success" << std::endl;
        return 1;
    }
"""

        sender = hexToInt(data["sender"])
        _hash = hexToInt(data["hash"])
        src += codeDoneRlp(sender, _hash);

    src += """
    return 0;
}
"""

    writeFile("/tmp/" + name + ".cpp", src)
    compileFile("/tmp/" + name + ".cpp", "/tmp/" + name);
    result = execFile("/tmp/" + name)
    if result != 0:
        _report("Test failure")

def gsTest(name, item, path):
    src = readFile("../src/evm.cpp")

    # implement

    writeFile("/tmp/" + name + ".cpp", src)
    compileFile("/tmp/" + name + ".cpp", "/tmp/" + name);
    result = execFile("/tmp/" + name)
    if result != 0:
        _report("Test failure")

def vmTests(filt):
    paths = listTests("./tests/VMTests", filePrefixes=[filt])
    for path in paths:
        data = readFile(path)
        for name, item in data.items():
            print(path, name)
            vmTest(name, item, path)

def txTests(filt):
    paths = listTests("./tests/TransactionTests", filePrefixes=[filt])
    for path in paths:
        data = readFile(path)
        for name, item in data.items():
            print(path, name)
            txTest(name, item, path)

def gsTests(filt):
    paths = listTests("./tests/GeneralStateTests", filePrefixes=[filt])
    for path in paths:
        data = readFile(path)
        for name, item in data.items():
            print(path, name)
            gsTest(name, item, path)

def main():
    filt = sys.argv[1] if len(sys.argv) == 2 else ""
#    try:
    vmTests(filt)
#    except: pass
#    try: txTests(filt)
#    except: pass
#    try: 
#    gsTests(filt)
#    except: pass

if __name__ == '__main__': main()
