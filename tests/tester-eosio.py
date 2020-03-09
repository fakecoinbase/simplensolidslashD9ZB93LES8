# pip3 install ecdsa pyyaml --user
import ecdsa, io, json, os, subprocess, sys, time, yaml

def derive_pk(e):
    signing_key = ecdsa.SigningKey.from_secret_exponent(e, curve=ecdsa.SECP256k1)
    x = signing_key.verifying_key.pubkey.point.x()
    y = signing_key.verifying_key.pubkey.point.y()
    return "%064x%064x" % (x, y)

def _report(*msg):
    print("\033[91m== " + sys.argv[0] + ":\x1b[0m", *msg, file=sys.stderr)

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
    with io.open(fname, "r", encoding="utf8") as f:
        fcontents = f.read()
        try:
            if fname.endswith(".json"):
                fparsed = json.loads(fcontents)
            elif fname.endswith(".yml"):
                fparsed = yaml.load(fcontents, Loader=yaml.FullLoader)
            elif fname.endswith(".cpp"):
                fparsed = fcontents
            elif fname.endswith(".hpp"):
                fparsed = fcontents
            else:
                _die("Do not know how to load:", fname)
            return fparsed
        except Exception as e:
            print(e)
            _die("Could not load file:", fname)

def writeFile(fname, fcontents):
    if not os.path.exists(os.path.dirname(fname)):
        os.makedirs(os.path.dirname(fname))
    with io.open(fname, "w", encoding="utf8") as f:
        f.write(fcontents)

def compileFile(fnamein, fnameout, fnamelog):
    if os.path.isfile(fnameout): return 0
    with io.open(fnamelog, "w", encoding="utf8") as f:
        return subprocess.call([
            "eosio-cpp",
            "-DNDEBUG",
            "-DNATIVE_CRYPTO",
            "-O=z",
            "-o",
            fnameout,
            fnamein,
        ], stdout=f, stderr=subprocess.STDOUT)

def execFile(fnamein, fnamelog):
    with io.open(fnamelog, "w", encoding="utf8") as f:
        result = subprocess.call([
            "./start-eosio-clean.sh", fnamein,
        ], stdout=f, stderr=subprocess.STDOUT)
        start = time.time()
        if result == 0:
            result = subprocess.call([
                "cleos", "push", "action", "evm", "test", "[]", "-p", "eosio@active",
            ], stdout=f, stderr=subprocess.STDOUT)
        end = time.time()
    ellapsed = int((end - start) * 1000)
    with io.open(fnamelog, "r", encoding="utf8") as f:
        content = f.read()
        if result != 0: print(content)
    if ellapsed > 150: print("\033[93m[" + str(ellapsed) + "ms]\x1b[0m")
    return result

def hexToInt(s):
    if s[:2] == "0x": s = s[2:]
    if s == "": s = "0"
    return int(s, 16)

def valToInt(s):
    if s[:2] == "0x": return hexToInt(s)
    if s == "": s = "0"
    return int(s)

def hexToBin(s):
    if s[:2] == "0x": s = s[2:]
    if len(s) > 2*256*1024: raise ValueError('too big')
    if len(s) > 0: int(s, 16) # validates
    hexv = ""
    hexs = ("0" if len(s) % 2 != 0 else "") + s
    for i in range(0, len(s) // 2):
        hexv += "\\x" + hexs[:2]
        hexs = hexs[2:]
    return hexv

def intToU256(v):
    if v < 0 or v > 2**256-1: _die("Not U256:", v)
    return "%064x" % v

def codeInitExec2(nonce, gasprice, gas, has_to, address, value, data, publickey):
    return """
    struct txn txn;

    txn.nonce = uhex256(\"""" + intToU256(nonce) + """\").cast64();
    txn.gasprice = uhex256(\"""" + intToU256(gasprice) + """\");
    txn.gaslimit = uhex256(\"""" + intToU256(gas) + """\").cast64();
    txn.has_to = """ + ("true" if has_to else "false") + """;
    txn.to = (uint160_t)uhex256(\"""" + intToU256(address) + """\");
    txn.value = uhex256(\"""" + intToU256(value) + """\");
    txn.data = (uint8_t*)\"""" + data + """\";
    txn.data_size = """ + str(len(data) // 4) + """;
    txn.is_signed = false;
    txn.v = 0;
    txn.r = 0;
    txn.s = 0;

    uint8_t *publickey = (uint8_t*)\"""" + publickey + """\";
    uint64_t publickey_size = """ + str(len(publickey) // 4) + """;

    uint160_t from = (uint160_t)sha3(publickey, publickey_size);
"""

def codeInitExec(origin, gasprice, address, caller, value, gas, code, data):
    return """
    uint160_t origin = (uint160_t)uhex256(\"""" + intToU256(origin) + """\");
    uint256_t gasprice = uhex256(\"""" + intToU256(gasprice) + """\");
    uint160_t address = (uint160_t)uhex256(\"""" + intToU256(address) + """\");
    uint160_t caller = (uint160_t)uhex256(\"""" + intToU256(caller) + """\");
    uint256_t value = uhex256(\"""" + intToU256(value) + """\");
    uint64_t gas = uhex256(\"""" + intToU256(gas) + """\").cast64();

    uint8_t *code = (uint8_t*)\"""" + code + """\";
    uint64_t code_size = """ + str(len(code) // 4) + """;
    uint8_t *data = (uint8_t*)\"""" + data + """\";
    uint64_t data_size = """ + str(len(data) // 4) + """;
"""

def codeInitEnv(number, timestamp, gaslimit, coinbase, difficulty):
    return """
    uint64_t number = uhex256(\"""" + intToU256(number) + """\").cast64();
    uint64_t timestamp = uhex256(\"""" + intToU256(timestamp) + """\").cast64();
    uint64_t gaslimit = uhex256(\"""" + intToU256(gaslimit) + """\").cast64();
    uint160_t coinbase = (uint160_t)uhex256(\"""" + intToU256(coinbase) + """\");
    uint256_t difficulty = uhex256(\"""" + intToU256(difficulty) + """\");
"""

def codeInitLocation(location, number):
    return """
        {
            uint256_t location = uhex256(\"""" + intToU256(location) + """\");
            uint256_t number = uhex256(\"""" + intToU256(number) + """\");
            state.store(account, location, number);

            check(state.load(account, location) == number, "location not set");
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
                eosio::print_f("post: invalid storage % % % %", to_string(account), to_string(location), to_string(_number), to_string(number));
                result = 1;
            }
        }
"""

def codeInitAccount(account, nonce, balance, code, storage):
    if len(code) // 4 > 24576: _die("code size exceeds maximum")
    src = """
    {
        uint160_t account = (uint160_t)uhex256(\"""" + intToU256(account) + """\");
        uint64_t nonce = uhex256(\"""" + intToU256(nonce) + """\").cast64();
        uint256_t balance = uhex256(\"""" + intToU256(balance) + """\");
        uint8_t *code1 = (uint8_t*)\"""" + code[0*4*6144:1*4*6144] + """\";
        uint8_t *code2 = (uint8_t*)\"""" + code[1*4*6144:2*4*6144] + """\";
        uint8_t *code3 = (uint8_t*)\"""" + code[2*4*6144:3*4*6144] + """\";
        uint8_t *code4 = (uint8_t*)\"""" + code[3*4*6144:4*4*6144] + """\";
        uint64_t code_size = """ + str(len(code) // 4) + """;
        local<uint8_t> code_l(code_size); uint8_t *code = code_l.data;
        for (uint64_t i = 0; i < code_size; i++) {
            if (i >= 3*6144) { code[i] = code4[i - 3*6144]; continue; }
            if (i >= 2*6144) { code[i] = code3[i - 2*6144]; continue; }
            if (i >= 1*6144) { code[i] = code2[i - 1*6144]; continue; }
            code[i] = code1[i];
        }
        uint256_t codehash = sha3(code, code_size);

        uint64_t acc_id = get_account(account);
        if (acc_id == 0) insert_account(account, 0, 0, 0);
        state.store_code(codehash, code, code_size);
        state.set_nonce(account, nonce);
        state.set_balance(account, balance);
        state.set_codehash(account, nonce > 0 || balance > 0 || code_size > 0 ? codehash : 0);

        check(state.get_nonce(account) == nonce, "nonce not set");
        check(state.get_balance(account) == balance, "balance not set");
        check(state.get_codehash(account) == (nonce > 0 || balance > 0 || code_size > 0 ? codehash : 0), "codehash not set");
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
"""

    if nonce != None:
        src += """
        uint256_t nonce = uhex256(\"""" + intToU256(nonce) + """\");
        if (nonce != state.get_nonce(account)) {
            eosio::print_f("post: invalid nonce");
            result = 1;
        }
"""

    if balance != None:
        src += """
        uint256_t balance = uhex256(\"""" + intToU256(balance) + """\");
        uint256_t _balance = state.get_balance(account);
        if (balance != _balance) {
            eosio::print_f("post: invalid balance % %", to_string(_balance), to_string(balance));
            result = 1;
        }
"""

    if code != None:
        src += """
        uint8_t *code = (uint8_t*)\"""" + code + """\";
        uint64_t code_size = """ + str(len(code) // 4) + """;

        uint256_t codehash = state.get_codehash(account);
        uint64_t _code_size;
        const uint8_t *_code = state.load_code(codehash, _code_size);
        if (code_size != _code_size) {
            eosio::print_f("post: invalid account code_size");
            result = 1;
        }
        for (uint64_t i = 0; i < _code_size; i++) {
            if (code[i] != _code[i]) {
                eosio::print_f("post: invalid account code");
                result = 1;
                break;
            }
        }
"""

    for subkey, subvalue in storage.items():
        location = hexToInt(str(subkey))
        number = valToInt(subvalue)
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
        eosio::print_f("post: invalid out_size");
        //for (uint64_t i = 0; i < out_size; i++) std::cerr << " " << (int)out_data[i];
        //std::cerr << std::endl;
        //for (uint64_t i = 0; i < return_size; i++) std::cerr << " " << (int)return_data[i];
        //std::cerr << std::endl;
        result = 1;
    }
    for (uint64_t i = 0; i < out_size; i++) {
        if (out_data[i] != return_data[i]) {
            eosio::print_f("post: invalid out_data");
            //for (uint64_t i = 0; i < out_size; i++) std::cerr << " " << (int)out_data[i];
            //std::cerr << std::endl;
            //for (uint64_t i = 0; i < out_size; i++) std::cerr << " " << (int)return_data[i];
            //std::cerr << std::endl;
            result = 1;
            break;
        }
    }
"""

def codeDoneRoot(roothash):
    return """
    uint256_t root = state.root("./trie/trie");
    uint256_t roothash = uhex256(\"""" + intToU256(roothash) + """\");
    if (root != roothash) {
        eosio::print_f("post: invalid root % %", to_string(root), to_string(roothash));
        result = 1;
    }
"""

def codeDoneLogs(loghash):
    return """
    uint256_t loghash = uhex256(\"""" + intToU256(loghash) + """\");
    uint256_t _loghash = state.loghash();
    if (loghash != _loghash) {
        eosio::print_f("post: invalid loghash % %", to_string(loghash), to_string(_loghash));
        result = 1;
    }
"""

def codeDoneGas(fgas):
    return """
    uint256_t fgas = uhex256(\"""" + intToU256(fgas) + """\");
    if (gas != fgas) {
        eosio::print_f("post: invalid gas % %", to_string(fgas), gas);
        result = 1;
    }
"""

def codeDoneRlp(_hash, sender):
    return """
    uint256_t _h = uhex256(\"""" + intToU256(_hash) + """\");
    if (h != _h) {
        eosio::print_f("post: invalid hash % %", to_string(h), to_string(_h));
        result = 1;
    }

    uint160_t _from = (uint160_t)uhex256(\"""" + intToU256(sender) + """\");
    if (from != _from) {
        eosio::print_f("post: invalid sender");
        result = 1;
    }
"""

def find_expect(expect, gas_index, value_index, data_index, release_name):
    if release_name == "Istanbul": release_name = '>=Istanbul'
    for item in expect:
        gas_indexes = item["indexes"]["gas"]
        if isinstance(gas_indexes, int) and gas_indexes > -1 and gas_index != gas_indexes: continue
        if isinstance(gas_indexes, list) and not gas_index in gas_indexes: continue
        value_indexes = item["indexes"]["value"]
        if isinstance(value_indexes, int) and value_indexes > -1 and value_index != value_indexes: continue
        if isinstance(value_indexes, list) and not value_index in value_indexes: continue
        data_indexes = item["indexes"]["data"]
        if isinstance(data_indexes, int) and data_indexes > -1 and data_index != data_indexes: continue
        if isinstance(data_indexes, list) and not data_index in data_indexes: continue
        networks = item["network"]
        if not release_name in networks: continue
        return item["result"]
    _die("Could not find expects")

def vmTest(name, item, path):
    src = ""

    src += """
class _Block : public Block {
private:
    uint64_t _number = 10000000;
    uint64_t _timestamp = 0;
    uint64_t _gaslimit = 10000000;
    uint160_t _coinbase = 0;
    uint256_t _difficulty = 17179869184;
public:
    _Block(uint64_t number, uint64_t timestamp, uint64_t gaslimit, const uint160_t& coinbase, const uint256_t& difficulty)
        : _number(number), _timestamp(timestamp), _gaslimit(gaslimit), _coinbase(coinbase), _difficulty(difficulty) {}
    uint64_t forknumber() { return _number; }
    uint64_t number() { return _number; }
    uint64_t timestamp() { return _timestamp; }
    uint64_t gaslimit() { return _gaslimit; }
    uint160_t coinbase() { return _coinbase; }
    uint256_t difficulty() { return _difficulty; }
    uint256_t hash(const uint256_t &number) {
        uint8_t buffer[32];
        uint256_t::to(number, buffer);
        return sha3(buffer, 32);
    }
};

[[eosio::action]]
void test() {
"""

    env = item["env"]
    number = hexToInt(env["currentNumber"])
    timestamp = hexToInt(env["currentTimestamp"])
    gaslimit = hexToInt(env["currentGasLimit"])
    coinbase = hexToInt(env["currentCoinbase"])
    difficulty = hexToInt(env["currentDifficulty"])
    src += codeInitEnv(number, timestamp, gaslimit, coinbase, difficulty)

    src += """
    _Block block(number, timestamp, gaslimit, coinbase, difficulty);
    State &state = *this;
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
        //if (std::getenv("EVM_DEBUG")) std::cerr << "vm success " << std::endl;
    }, Error e, {
        success = false;
        gas = 0;
        //if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
    })
    storage.end(snapshot, success);
    // gas seems not to be detucted on vm Tests
    storage.flush();
    int result = 0;
"""

    if not "out" in item:

        src += """
    if (success) {
        eosio::print_f("post: invalid success on failure");
        result = 1;
    }
"""

    else:

        src += """
    if (!success) {
        eosio::print_f("post: invalid failure on success");
        result = 1;
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

#        loghash = hexToInt(item["logs"])
#        src += codeDoneLogs(loghash);

        fgas = hexToInt(item["gas"])
        src += codeDoneGas(fgas)

    src += """
    check(result == 0, "execution failure");
}
"""

    src = readFile("../contracts/evm/evm.cpp").replace("using contract::contract;", "using contract::contract;\n\n\n" + src)
    hdr = readFile("../src/evm.hpp")
    src = src.replace("#include \"../../src/evm.hpp\"", hdr)

    filename = "cache/vm/" + path.split('/')[-2] + "/eosio." + name + "/evm/"
    writeFile(filename + "evm.cpp", src)
    result = compileFile(filename + "evm.cpp", filename + "evm.wasm", filename + "evm.log")
    if result != 0: _report("Test fail to compile"); return
    result = execFile(filename, filename + "evm.log")
    if result != 0: _report("Test failure")#; subprocess.call(["rm", "-rf", filename])

def ttTest(name, item, path):
#    print(json.dumps(item, indent=2))

    try: rlp = hexToBin(item["rlp"])
    except: return

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
        if not release_name in item: continue

        print(release_name)

        src = ""

        data = item[release_name]

        src += """
[[eosio::action]]
void test() {
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
        _catches(verify_txn)(release, txn);
        uint256_t h2 = _catches(hash_txn)(txn);
        //if (std::getenv("EVM_DEBUG")) {
            //std::cerr << txn.nonce << std::endl;
            //std::cerr << txn.gasprice << std::endl;
            //std::cerr << txn.gaslimit << std::endl;
            //std::cerr << txn.has_to << std::endl;
            //std::cerr << txn.to << std::endl;
            //std::cerr << txn.value << std::endl;
            //std::cerr << txn.data_size << std::endl;
            //std::cerr << txn.is_signed << std::endl;
            //std::cerr << txn.v << std::endl;
            //std::cerr << txn.r << std::endl;
            //std::cerr << txn.s << std::endl;
        //}
        from = _catches(ecrecover)(h2, txn.v, txn.r, txn.s);
        uint64_t gas = (txn.gaslimit >> 64) > 0 ? ~(uint64_t)0 : txn.gaslimit.cast64();
        _catches(consume_gas)(gas, gas_intrinsic(release, txn.has_to, txn.data, txn.data_size));
    }, Error e, {
        success = false;
        //if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
    })
    int result = 0;
"""
        if not "hash" in data:

            src += """
    if (success) {
        eosio::print_f("post: invalid success on failure");
        result = 1;
    }
"""

        else:

            src += """
    if (!success) {
        eosio::print_f("post: invalid failure on success");
        result = 1;
    }
"""
            _hash = hexToInt(data["hash"])
            sender = hexToInt(data["sender"])

            src += codeDoneRlp(_hash, sender);

        src += """
    check(result == 0, "execution failure");
}
"""

        src = readFile("../contracts/evm/evm.cpp").replace("using contract::contract;", "using contract::contract;\n\n\n" + src)
        hdr = readFile("../src/evm.hpp")
        src = src.replace("#include \"../../src/evm.hpp\"", hdr)

        filename = "cache/tt/" + path.split('/')[-2] + "/eosio." + name + "_" + release + "/evm/"
        writeFile(filename + "evm.cpp", src)
        result = compileFile(filename + "evm.cpp", filename + "evm.wasm", filename + "evm.log")
        if result != 0: _report("Test fail to compile"); continue
        result = execFile(filename, filename + "evm.log")
        if result != 0: _report("Test failure")#; subprocess.call(["rm", "-rf", filename])

def stTest(name, item, path):
#    print(json.dumps(item, indent=2))

    post = item["post"]

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
        if not release_name in post: continue

        print(release_name)

        tests = post[release_name]

        for num, test in enumerate(tests):
            print("Test " + str(num))

            src = ""

            src += """
class _Block : public Block {
private:
    uint64_t _number = 10000000;
    uint64_t _timestamp = 0;
    uint64_t _gaslimit = 10000000;
    uint160_t _coinbase = 0;
    uint256_t _difficulty = 17179869184;
public:
    _Block(uint64_t number, uint64_t timestamp, uint64_t gaslimit, const uint160_t& coinbase, const uint256_t& difficulty)
        : _number(number), _timestamp(timestamp), _gaslimit(gaslimit), _coinbase(coinbase), _difficulty(difficulty) {}
    uint64_t forknumber() { return _number; }
    uint64_t number() { return _number; }
    uint64_t timestamp() { return _timestamp; }
    uint64_t gaslimit() { return _gaslimit; }
    uint160_t coinbase() { return _coinbase; }
    uint256_t difficulty() { return _difficulty; }
    uint256_t hash(const uint256_t &number) {
        uint8_t buffer[32];
        uint256_t::to(number, buffer);
        return sha3(buffer, 32);
    }
};

[[eosio::action]]
void test() {
"""

            env = item["env"]
            number = hexToInt(env["currentNumber"])
            timestamp = hexToInt(env["currentTimestamp"])
            gaslimit = hexToInt(env["currentGasLimit"])
            coinbase = hexToInt(env["currentCoinbase"])
            difficulty = hexToInt(env["currentDifficulty"])
            src += codeInitEnv(number, timestamp, gaslimit, coinbase, difficulty)

            src += """
    _Block block(number, timestamp, gaslimit, coinbase, difficulty);
    State &state = *this;
    Release release = """ + release + """;
    Storage storage(&state);
"""

            txn = item["transaction"]
            sk = hexToInt(txn['secretKey'])
            pk = derive_pk(sk);

            gas_index = test["indexes"]["gas"]
            value_index = test["indexes"]["value"]
            data_index = test["indexes"]["data"]

            nonce = hexToInt(txn["nonce"])
            gasprice = hexToInt(txn["gasPrice"])
            gas = hexToInt(txn["gasLimit"][gas_index])
            has_to = txn["to"] != ""
            address = hexToInt(txn["to"])
            value = hexToInt(txn["value"][value_index])
            data = hexToBin(txn['data'][data_index])
            publickey = hexToBin(pk)
            src += codeInitExec2(nonce, gasprice, gas, has_to, address, value, data, publickey)

            pre = item["pre"]
            for key, values in pre.items():
                account = hexToInt(key)
                nonce = hexToInt(values["nonce"])
                balance = hexToInt(values["balance"])
                code = hexToBin(values['code']);
                storage = values["storage"];
                src += codeInitAccount(account, nonce, balance, code, storage)

            src += """
    bool pays_gas = true;
    _try({
        if (txn.nonce != storage.get_nonce(from)) _trythrow(NONCE_MISMATCH);
        uint160_t to = txn.has_to ? txn.to : _catches(gen_contract_address)(from, storage.get_nonce(from));
        storage.increment_nonce(from);

        if (txn.gaslimit > block.gaslimit()) _trythrow(OUTOFBOUNDS_VALUE);
        uint64_t gas = txn.gaslimit.cast64();
        _catches(consume_gas)(gas, gas_intrinsic(release, txn.has_to, txn.data, txn.data_size));
        uint256_t gas_cost = txn.gaslimit * txn.gasprice;
        if (pays_gas) {
            if (storage.get_balance(from) < gas_cost) _trythrow(INSUFFICIENT_BALANCE);
            storage.sub_balance(from, gas_cost);
        }
        if (storage.get_balance(from) < txn.value) _trythrow(INSUFFICIENT_BALANCE);

        uint64_t return_size = 0;
        uint64_t return_capacity = 0;
        uint8_t *return_data = nullptr;

        bool success;
        uint64_t snapshot = storage.begin();
        if (txn.has_to) { // message call
            uint64_t code_size;
            uint8_t *code = storage.get_call_code(to, code_size);
            _try({
                if (!storage.exists(to)) storage.create_account(to, false);
                storage.sub_balance(from, txn.value);
                storage.add_balance(to, txn.value);
                success = _catches(vm_run)(release, block, storage,
                                from, txn.gasprice,
                                to, code, code_size,
                                from, txn.value, txn.data, txn.data_size,
                                return_data, return_size, return_capacity, gas,
                                false, 0);
            }, Error e, {
                success = false;
                gas = 0;
                //if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
            })
            storage.release_code(code);
        } else { // contract creation
            _try({
                if (storage.has_contract(to)) _trythrow(CODE_CONFLICT);
                storage.create_account(to, true);
                if (release >= SPURIOUS_DRAGON) storage.set_nonce(to, 1);
                storage.sub_balance(from, txn.value);
                storage.add_balance(to, txn.value);
                success = _catches(vm_run)(release, block, storage,
                                from, txn.gasprice,
                                to, txn.data, txn.data_size,
                                from, txn.value, nullptr, 0,
                                return_data, return_size, return_capacity, gas,
                                false, 0);
                if (success) {
                    _catches(code_size_check)(release, return_size);
                    _try({
                        _catches(consume_gas)(gas, gas_create(release, return_size));
                        storage.register_code(to, return_data, return_size);
                    }, Error e ,{
                        if (release >= HOMESTEAD) _trythrow(e);
                    })
                }
            }, Error e, {
                success = false;
                gas = 0;
                //if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
            })
        }
        storage.end(snapshot, success);

        _delete(return_data);

        uint64_t refund_gas = storage.get_refund();
        uint64_t used_gas_before_refund = txn.gaslimit.cast64() - gas;
        credit_gas(gas, _min(refund_gas, used_gas_before_refund / 2));
        uint64_t used_gas = txn.gaslimit.cast64() - gas;
        if (pays_gas) {
            storage.add_balance(from, gas * txn.gasprice);
            storage.add_balance(block.coinbase(), used_gas * txn.gasprice);
        }

        storage.flush();
    }, Error e, {
        //if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
    })
    int result = 0;
"""

#            loghash = hexToInt(test["logs"])
#            src += codeDoneLogs(loghash);

            result = find_expect(item['expect'], gas_index, value_index, data_index, release_name)
            for key, values in result.items():
                if key == "//comment": continue
                account = hexToInt(str(key))
                nonce = valToInt(values["nonce"]) if "nonce" in values else None
                balance = valToInt(values["balance"]) if "balance" in values else None
                code = hexToBin(values['code']) if "code" in values else None
                storage = values["storage"] if "storage" in values else {}
                src += codeDoneAccount(account, nonce, balance, code, storage)

#            roothash = hexToInt(test["hash"])
#            src += codeDoneRoot(roothash)

            src += """
    check(result == 0, "execution failure");
}
"""

            src = readFile("../contracts/evm/evm.cpp").replace("using contract::contract;", "using contract::contract;\n\n\n" + src)
            hdr = readFile("../src/evm.hpp")
            src = src.replace("#include \"../../src/evm.hpp\"", hdr)

            filename = "cache/st/" + path.split('/')[-2] + "/eosio." + name + "_" + release + "_" + str(num) + "/evm/"
            writeFile(filename + "evm.cpp", src)
            result = compileFile(filename + "evm.cpp", filename + "evm.wasm", filename + "evm.log")
            if result != 0: _report("Test fail to compile"); return
            result = execFile(filename, filename + "evm.log")
            if result != 0: _report("Test failure")#; subprocess.call(["rm", "-rf", filename])

def vmTests(filt):
    paths = listTests("../support/tests/VMTests", filePrefixes=[filt])
    for path in paths:
        data = readFile(path)
        for name, item in data.items():
            print(path, name)
            vmTest(name, item, path)

def ttTests(filt):
    paths = listTests("../support/tests/TransactionTests", filePrefixes=[filt])
    for path in paths:
        data = readFile(path)
        for name, item in data.items():
            print(path, name)
            ttTest(name, item, path)

def stTests(filt):
    paths = listTests("../support/tests/GeneralStateTests", filePrefixes=[filt])
    for path in paths:
        data = readFile(path)
        path_filler = path.replace('/GeneralStateTests/', '/src/GeneralStateTestsFiller/').replace('.json', 'Filler.json')
        if not os.path.isfile(path_filler):
            path_filler = path.replace('/GeneralStateTests/', '/src/GeneralStateTestsFiller/').replace('.json', 'Filler.yml')
        data_filler = readFile(path_filler)
        for name, item in data.items():
            print(path, name)
            item['expect'] = data_filler[name]['expect']
            stTest(name, item, path)

def main():
    filt = sys.argv[1] if len(sys.argv) == 2 else ""
    vmTests(filt)
    ttTests(filt)
    stTests(filt)

if __name__ == '__main__': main()
