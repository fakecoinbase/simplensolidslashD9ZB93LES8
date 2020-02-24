#pip install ecdsa --user
import ecdsa, json, os, subprocess, sys

#signing_key = ecdsa.SigningKey.from_secret_exponent(e, curve=ecdsa.SECP256k1)
#return (signing_key.verifying_key.pubkey.point.x(), signing_key.verifying_key.pubkey.point.y())

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
        "-Wno-unused-function",
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
    return "%064x" % v

def codeInitExec(origin, gasprice, address, caller, value, gas, code, data):
    return """
    uint160_t origin = (uint160_t)uhex256(\"""" + intToU256(origin) + """\");
    uint256_t gasprice = uhex256(\"""" + intToU256(gasprice) + """\");
    uint160_t address = (uint160_t)uhex256(\"""" + intToU256(address) + """\");
    uint160_t caller = (uint160_t)uhex256(\"""" + intToU256(caller) + """\");
    uint256_t value = uhex256(\"""" + intToU256(value) + """\");
    uint256_t _gas = uhex256(\"""" + intToU256(gas) + """\");

    uint64_t gas = _gas.cast64(); // fix

    uint8_t *code = (uint8_t*)\"""" + code + """\";
    uint64_t code_size = """ + str(len(code) // 4) + """;
    uint8_t *data = (uint8_t*)\"""" + data + """\";
    uint64_t data_size = """ + str(len(data) // 4) + """;
"""

def codeInitEnv(timestamp, number, gaslimit, difficulty, coinbase):
    return """
    uint64_t timestamp = uhex256(\"""" + intToU256(timestamp) + """\").cast64();
    uint64_t number = uhex256(\"""" + intToU256(number) + """\").cast64();
    uint64_t gaslimit = uhex256(\"""" + intToU256(gaslimit) + """\").cast64();
    uint64_t difficulty = uhex256(\"""" + intToU256(difficulty) + """\").cast64();
    uint160_t coinbase = (uint160_t)uhex256(\"""" + intToU256(coinbase) + """\");
"""

def codeInitLocation(location, number):
    return """
        {
            uint256_t location = uhex256(\"""" + intToU256(location) + """\");
            uint256_t number = uhex256(\"""" + intToU256(number) + """\");
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
            uint256_t location = uhex256(\"""" + intToU256(location) + """\");
            uint256_t number = uhex256(\"""" + intToU256(number) + """\");
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
        uint160_t account = (uint160_t)uhex256(\"""" + intToU256(account) + """\");
        uint64_t nonce = uhex256(\"""" + intToU256(nonce) + """\").cast64();
        uint256_t balance = uhex256(\"""" + intToU256(balance) + """\");
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
        uint160_t account = (uint160_t)uhex256(\"""" + intToU256(account) + """\");
        uint256_t nonce = uhex256(\"""" + intToU256(nonce) + """\");
        uint256_t balance = uhex256(\"""" + intToU256(balance) + """\");
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
    uint256_t loghash = uhex256(\"""" + intToU256(loghash) + """\");
"""

def codeDoneGas(fgas):
    return """
    uint256_t fgas = uhex256(\"""" + intToU256(fgas) + """\");
    if (gas != fgas) {
//        std::cerr << "post: invalid gas " << fgas << " " << gas << std::endl;
//        return 1;
    }
"""

def codeDoneRlp(_hash, sender):
    return """
    uint256_t _h = uhex256(\"""" + intToU256(_hash) + """\");
    if (h != _h) {
        std::cerr << "post: invalid hash " << h << " " << _h << std::endl;
        return 1;
    }

    uint160_t _from = (uint160_t)uhex256(\"""" + intToU256(sender) + """\");
    if (from != _from) {
        std::cerr << "post: invalid sender" << std::endl;
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
    gaslimit = hexToInt(env["currentGasLimit"])
    difficulty = hexToInt(env["currentDifficulty"])
    coinbase = hexToInt(env["currentCoinbase"])
    src += codeInitEnv(timestamp, number, gaslimit, difficulty, coinbase)

    src += """
    _Block block(timestamp, number, gaslimit, difficulty, coinbase);
    _State state;
    Storage storage(&state);

    Release release = get_release(block.forknumber());
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

    filename = "/tmp/tx_" + name
    writeFile(filename + ".cpp", src)
    compileFile(filename + ".cpp", filename);
    result = execFile(filename)
    if result != 0: _report("Test failure")

def txTest(name, item, path):
#    print(json.dumps(item, indent=2))

    rlp = hexToBin(item["rlp"])

    releases = {
      "Frontier": "FRONTIER",
      "Homestead": "HOMESTEAD",
      "EIP150": "TANGERINE_WHISTLE",
      "EIP158": "SPURIOUS_DRAGON",
      "Byzantium": "BYZANTIUM",
      "Constantinople": "CONSTANTINOPLE",
      "ConstantinopleFix": "PETERSBURG",
      "Istanbul": "ISTANBUL",
    }

    for release_name, release in releases.items():
        print(release_name)

        src = readFile("../src/evm.cpp")
        hdr = readFile("../src/evm.hpp")
        src = src.replace("#include \"evm.hpp\"", hdr)

        data = item[release_name]

        src += """
int main()
{
    Release release = """ + release + """;
"""

        src += codeInitRlp(rlp);

        src += """
    uint256_t h = 0;
    uint160_t from = 0;
    bool success = true;
    _try({
        h = sha3(buffer, size);
        struct txn txn;
        _catches(decode_txn)(buffer, size, txn);
        _catches(_verify_txn)(release, txn);
        uint256_t h2 = _catches(_txn_hash)(txn);
        if (std::getenv("EVM_DEBUG")) {
            std::cerr << txn.nonce << std::endl;
            std::cerr << txn.gasprice << std::endl;
            std::cerr << txn.gaslimit << std::endl;
            std::cerr << txn.has_to << std::endl;
            std::cerr << txn.to << std::endl;
            std::cerr << txn.value << std::endl;
            std::cerr << txn.data_size << std::endl;
            std::cerr << txn.is_signed << std::endl;
            std::cerr << txn.v << std::endl;
            std::cerr << txn.r << std::endl;
            std::cerr << txn.s << std::endl;
        }
        from = _catches(ecrecover)(h2, txn.v, txn.r, txn.s);
    }, Error e, {
        success = false;
        if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
    })
"""
        if not "hash" in data:

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
            _hash = hexToInt(data["hash"])
            sender = hexToInt(data["sender"])

            src += codeDoneRlp(_hash, sender);

        src += """
    return 0;
}
"""
        filename = "/tmp/tx_" + name + "_" + release
        writeFile(filename + ".cpp", src)
        compileFile(filename + ".cpp", filename);
        result = execFile(filename)
        if result != 0: _report("Test failure")

def gsTest(name, item, path):
    src = readFile("../src/evm.cpp")
    hdr = readFile("../src/evm.hpp")
    src = src.replace("#include \"evm.hpp\"", hdr)

    # implement
    print(json.dumps(item, indent=2))

    writeFile("/tmp/" + name + ".cpp", src)
    compileFile("/tmp/" + name + ".cpp", "/tmp/" + name);
    result = execFile("/tmp/" + name)
    if result != 0: _report("Test failure")

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
    try: vmTests(filt)
    except: pass
    try: txTests(filt)
    except: pass
#    try: gsTests(filt)
#    except: pass

if __name__ == '__main__': main()
