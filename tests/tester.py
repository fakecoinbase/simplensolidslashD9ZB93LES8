#pip3 install ecdsa --user
#pip3 install pyyaml --user
import ecdsa, json, os, subprocess, sys, yaml


def derive_pk(e):
    signing_key = ecdsa.SigningKey.from_secret_exponent(e, curve=ecdsa.SECP256k1)
    x = signing_key.verifying_key.pubkey.point.x()
    y = signing_key.verifying_key.pubkey.point.y()
    return "%064x%064x" % (x, y)

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
                fparsed, _ = fcontents.split("// ** main **")
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
    if os.path.isfile(fnameout): return 0;
    return subprocess.call([
        "g++",
        "-std=c++11",
        "-pedantic",
        "-Wall",
        "-Wno-vla",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Wno-unused-function",
        "-Wno-maybe-uninitialized",
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

def valToInt(s):
    if s[:2] == "0x": return hexToInt(s)
    if s == "": s = "0"
    return int(s)

def hexToBin(s):
    if s[:2] == "0x": s = s[2:]
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

def codeInitExec2(nonce, gasprice, gas, address, value, data, publickey):
    return """
    struct txn txn;

    txn.nonce = uhex256(\"""" + intToU256(nonce) + """\").cast64();
    txn.gasprice = uhex256(\"""" + intToU256(gasprice) + """\");
    txn.gaslimit = uhex256(\"""" + intToU256(gas) + """\").cast64();
    txn.has_to = true;
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
                std::cerr << "post: invalid storage " << _number << " " << number << std::endl;
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
"""

    if nonce != None:
        src += """
        uint256_t nonce = uhex256(\"""" + intToU256(nonce) + """\");
        if (nonce != state.get_nonce(account)) {
            std::cerr << "post: invalid nonce" << std::endl;
            return 1;
        }
"""

    if balance != None:
        src += """
        uint256_t balance = uhex256(\"""" + intToU256(balance) + """\");
        if (balance != state.get_balance(account)) {
            std::cerr << "post: invalid balance" << std::endl;
            return 1;
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
        location = hexToInt(str(subkey))
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

def codeDoneRoot(roothash):
    return """
    uint256_t root = state.root("./trie/trie");
    uint256_t roothash = uhex256(\"""" + intToU256(roothash) + """\");
    if (root != roothash) {
        std::cerr << "post: invalid root " << root << " " << roothash << std::endl;
        return 1;
    }
"""

def codeDoneLogs(loghash):
    return """
    uint256_t loghash = uhex256(\"""" + intToU256(loghash) + """\");
    uint256_t _loghash = state.loghash();
    if (loghash != _loghash) {
        std::cerr << "post: invalid loghash " << loghash << " " << _loghash << std::endl;
        return 1;
    }
"""

def codeDoneGas(fgas):
    return """
    uint256_t fgas = uhex256(\"""" + intToU256(fgas) + """\");
    if (gas != fgas) {
        std::cerr << "post: invalid gas " << fgas << " " << gas << std::endl;
        return 1;
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

def find_expect(expect, gas_index, value_index, data_index, release_name):
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
        if release_name == "Frontier" and not "Frontier" in networks: continue
        if release_name != "Istanbul" and not "Frontier" in networks and not "Homestead" in networks: continue
        return item["result"]
    _die("Could not find expects")

def vmTest(name, item, path):
    src = readFile("../src/evm.cpp")
    hdr = readFile("../src/evm.hpp")
    src = src.replace("#include \"evm.hpp\"", hdr)

    src += """
int main()
{
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
    credit_gas(gas, _min(refund_gas, used_gas / 2));
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

    filename = "/tmp/vm_" + name
    writeFile(filename + ".cpp", src)
    result = compileFile(filename + ".cpp", filename)
    if result != 0: _report("Test fail to compile"); return
    result = execFile(filename)
    if result != 0: _report("Test failure")

def txTest(name, item, path):
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
        _catches(verify_txn)(release, txn);
        uint256_t h2 = _catches(hash_txn)(txn);
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
        result = compileFile(filename + ".cpp", filename)
        if result != 0: _report("Test fail to compile"); continue
        result = execFile(filename)
        if result != 0: _report("Test failure")

def gsTest(name, item, path):
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

            src = readFile("../src/evm.cpp")
            hdr = readFile("../src/evm.hpp")
            src = src.replace("#include \"evm.hpp\"", hdr)

            src += """
int main()
{
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
    _State state;
    Storage storage(&state);

    Release release = """ + release + """;
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
            address = hexToInt(txn["to"])
            value = hexToInt(txn["value"][value_index])
            data = hexToBin(txn['data'][data_index])
            publickey = hexToBin(pk)
            src += codeInitExec2(nonce, gasprice, gas, address, value, data, publickey)

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
    bool success = false;
    _try({
        if (txn.nonce != storage.get_nonce(from)) _trythrow(NONCE_MISMATCH);
        storage.increment_nonce(from);
        uint160_t to = txn.has_to ? txn.to : _catches(gen_contract_address)(from, storage.get_nonce(from));

        // check for overflow
        uint64_t gas = txn.gaslimit.cast64();
        _catches(consume_gas)(gas, gas_intrinsic(release, txn.has_to, txn.data, txn.data_size));
        uint256_t gas_cost = txn.gaslimit * txn.gasprice;
        if (pays_gas) {
            if (storage.get_balance(from) < gas_cost) _trythrow(INSUFFICIENT_BALANCE);
            storage.sub_balance(from, gas_cost);
        }

        uint64_t return_size = 0;
        uint64_t return_capacity = 0;
        uint8_t *return_data = nullptr;

        uint64_t snapshot = storage.begin();
        _try({
            if (storage.get_balance(from) < txn.value) _trythrow(INSUFFICIENT_BALANCE);
            if (txn.has_to) { // message call
                uint64_t code_size;
                const uint8_t *code = storage.get_code(to, code_size);
                if (!storage.exists(to)) {
                    if (release >= SPURIOUS_DRAGON) {
                        if (txn.value > 0) {
                            if ((intptr_t)code > BLAKE2F) {
                                goto skip;
                            }
                        }
                    }
                    storage.create_account(to);
                }
                storage.sub_balance(from, txn.value);
                storage.add_balance(to, txn.value);
                success = _catches(vm_run)(release, block, storage,
                                from, txn.gasprice,
                                to, code, code_size,
                                from, txn.value, txn.data, txn.data_size,
                                return_data, return_size, return_capacity, gas,
                                false, 0);
                _delete(code);
            } else { // contract creation
                if (storage.has_contract(to)) _trythrow(CODE_CONFLICT);
                storage.create_account(to);
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
                    _catches(consume_gas)(gas, gas_create(release, return_size));
                    storage.register_code(to, return_data, return_size);
                }
            }
        }, Error e, {
            success = false;
            gas = 0;
            if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
        })
    skip:
        storage.end(snapshot, success);

        _delete(return_data);

        uint64_t refund_gas = storage.get_refund();
        uint64_t used_gas = txn.gaslimit.cast64() - gas;
        credit_gas(gas, _min(refund_gas, used_gas / 2));
        if (pays_gas) storage.add_balance(from, gas * txn.gasprice);

        storage.flush();
    }, Error e, {
        success = false;
        if (std::getenv("EVM_DEBUG")) std::cerr << "vm exception " << errors[e] << std::endl;
    })
"""

            result = find_expect(item['expect'], gas_index, value_index, data_index, release_name)

#            print(json.dumps(result, indent=2))

            if not "hash" in test:

                src += """
    if (success) {
//        std::cerr << "post: invalid success on failure" << std::endl;
//        return 1;
    }
"""

            else:

                src += """
    if (!success) {
//        std::cerr << "post: invalid failure on success" << std::endl;
//        return 1;
    }
"""

                loghash = hexToInt(test["logs"])
                src += codeDoneLogs(loghash);

                for key, values in result.items():
                    if key == "//comment": continue
                    account = hexToInt(str(key))
                    nonce = valToInt(values["nonce"]) if "nonce" in values else None
                    balance = valToInt(values["balance"]) if "balance" in values else None
                    code = hexToBin(values['code']) if "code" in values else None
                    storage = values["storage"] if "storage" in values else {}
                    src += codeDoneAccount(account, nonce, balance, code, storage)

#                roothash = hexToInt(test["hash"])
#                src += codeDoneRoot(roothash)

            src += """
    return 0;
}
"""

            filename = "/tmp/gs_" + name + "_" + release + "_" + str(num)
            writeFile(filename + ".cpp", src)
            result = compileFile(filename + ".cpp", filename)
            if result != 0: _report("Test fail to compile"); return
            result = execFile(filename)
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
        path_filler = path.replace('/GeneralStateTests/', '/src/GeneralStateTestsFiller/').replace('.json', 'Filler.json')
        if not os.path.isfile(path_filler):
            path_filler = path.replace('/GeneralStateTests/', '/src/GeneralStateTestsFiller/').replace('.json', 'Filler.yml')
        data_filler = readFile(path_filler)
        for name, item in data.items():
            print(path, name)
            item['expect'] = data_filler[name]['expect']
            gsTest(name, item, path)

def main():
    filt = sys.argv[1] if len(sys.argv) == 2 else ""
    vmTests(filt)
    txTests(filt)
    gsTests(filt)

if __name__ == '__main__': main()
