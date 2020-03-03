import binascii, math, os, sys

h2b = lambda s: binascii.unhexlify(('0' if len(s) % 2 != 0 else '') + s)
b2h = lambda b: binascii.hexlify(b).decode()

h2n = lambda s: 0 if s == '' else int(s, 16)
n2h = lambda n, l=0: '%%0%dx' % (2*l) % n if n > 0 or l > 0 else ''

b2n = lambda b: h2n(b2h(b))
n2b = lambda n, l=0: h2b(n2h(n, l))

def keccak(message, r, c, n):
    b = r + c
    k = b // 25
    l = int(math.log(k, 2))

    assert r % 8 == 0
    assert c % 8 == 0
    assert n % 8 == 0
    assert b % 25 == 0
    assert k % 8 == 0

    _b2n = lambda b: b2n(b[::-1])
    _n2b = lambda w, n: n2b(w, n)[::-1]

    chunks = lambda l, n: [l[i:i+n] for i in range(0, len(l), n)]

    ROT = lambda x, y, k: ((x >> (k - y % k)) ^ (x << y % k)) % (1 << k)

    bytesize = len(message)
    bitsize = 8 * bytesize
    padding = (r - bitsize % r) // 8
    message += b'\x01' + (padding - 2) * b'\0' + b'\x80' if padding > 1 else b'\x81'

    assert len(message) % (r // 8) == 0

    ws = list(map(_b2n, chunks(message, k // 8)))
    s = [5 * [0], 5 * [0], 5 * [0], 5 * [0], 5 * [0]]
    RC = [
        0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
        0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
        0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
    ]
    R = [
        [ 0, 36,  3, 41, 18],
        [ 1, 44, 10, 45,  2],
        [62,  6, 43, 15, 61],
        [28, 55, 25, 21, 56],
        [27, 20, 39,  8, 14],
    ]
    rounds = 12 + 2 * l
    for w in chunks(ws, r // k):
        w += (c // k) * [0]
        for y in range(0, 5):
            for x in range(0, 5):
                s[x][y] ^= w[5 * y + x]
        for j in range(0, rounds):
            C = 5 * [0]
            for x in range(0, 5):
                C[x] = s[x][0] ^ s[x][1] ^ s[x][2] ^ s[x][3] ^ s[x][4]
            D = 5 * [0]
            for x in range(0, 5):
                D[x] = C[(x - 1) % 5] ^ ROT(C[(x + 1) % 5], 1, k)
            for x in range(0, 5):
                for y in range(0, 5):
                    s[x][y] ^= D[x]
            B = [5 * [0], 5 * [0], 5 * [0], 5 * [0], 5 * [0]]
            for x in range(0, 5):
                for y in range(0, 5):
                    B[y][(2 * x + 3 * y) % 5] = ROT(s[x][y], R[x][y], k)
            for x in range(0, 5):
                for y in range(0, 5):
                    s[x][y] = B[x][y] ^ (~B[(x + 1) % 5][y] & B[(x + 2) % 5][y])
            s[0][0] ^= RC[j]
    Z = b''
    while len(Z) < n // 8:
        for y in range(0, 5):
            for x in range(0, 5):
                Z += _n2b(s[x][y], k // 8)
    return Z[:n // 8]

keccak256 = lambda message: keccak(message, r=1088, c=512, n=256)

# encoding of 8-bit integers
def int8(n, r=True):
    if n < 0 or n >= 2**8: raise ValueError('Invalid constant')
    return n2b(n, 1)

# decoding of 8-bit integers
def parse_int8(b, r=True):
    if len(b) < 1: raise ValueError('End of input')
    n = b2n(b[:1])
    b = b[1:]
    return n, b

# encoding of non-leading-zero integers
def nlzint(n):
    if n < 0 or n >= 2**256: raise ValueError('Invalid constant')
    if n == 0: return b''
    s = n2h(n)
    if len(s) % 2 == 1: s = '0' + s
    return h2b(s)

# rlp encoding
def rlp(v):
    def varlen(base, n):
        if n < 0 or n >= 2**64: raise ValueError('Invalid length')
        if n > 55:
            b = nlzint(n)
            return int8(base + 55 + len(b)) + b
        return int8(base + n)
    if isinstance(v, bytes):
        b = v
        if len(b) == 1:
            n = parse_int8(b)[0]
            if n < 0x80: return b
        return varlen(0x80, len(b)) + b
    if isinstance(v, list):
        b = b''.join(map(rlp, v))
        return varlen(0xc0, len(b)) + b
    raise ValueError('Unsupported datatype')

# serialize a transaction
def txn_encode(fields={}):
    nonce = fields['nonce'] if 'nonce' in fields else 0
    gasprice = fields['gasprice'] if 'gasprice' in fields else 0
    gaslimit = fields['gaslimit'] if 'gaslimit' in fields else 0
    to = fields['to'] if 'to' in fields else None
    value = fields['value'] if 'value' in fields else 0
    data = fields['data'] if 'data' in fields else b''
    v = fields['v'] if 'v' in fields else b''
    r = fields['r'] if 'r' in fields else b''
    s = fields['s'] if 's' in fields else b''
    b = b''
    if to != None:
        b = h2b(to)
        if len(b) != 20: raise ValueError('Invalid length')
    return rlp(
        [ nlzint(nonce), nlzint(gasprice), nlzint(gaslimit), b, nlzint(value), data ] +
        ([ nlzint(b2n(v)), nlzint(b2n(r)), nlzint(b2n(s)) ] if v+r+s != b'' else [])
    )

# generates a contract creation transaction
def txn_contract(nonce, binfile, supply=None):
    with open(binfile, 'r') as f: hexdata = f.read()
    data = h2b(hexdata) + (n2b(supply, 32) if supply else b'')
    fields = {}
    fields['nonce'] = nonce
    fields['gasprice'] = 1
    fields['gaslimit'] = 10000000
    fields['data'] = data
    fields['v'] = n2b(27)
    return txn_encode(fields);

# computes a contract address
def txn_address(address, nonce):
    if address[:2] == '0x': address = address[2:]
    if len(address) != 40: raise ValueError('invalid address')
    return keccak256(rlp([h2b(address), nlzint(nonce)]))[12:]

# generates a wrap token transaction
def txn_wrap(nonce, contract, amount):
    if contract[:2] == '0x': contract = contract[2:]
    if len(contract) != 40: raise ValueError('invalid address')
    funsig = 'wrap()'
    method = keccak256(funsig.encode())[:4]
    data = method
    fields = {}
    fields['nonce'] = nonce
    fields['gasprice'] = 1
    fields['gaslimit'] = 100000
    fields['to'] = contract
    fields['data'] = data
    fields['value'] = amount
    fields['v'] = n2b(27)
    return txn_encode(fields);

# generates an unwrap token transaction
def txn_unwrap(nonce, contract, amount):
    if contract[:2] == '0x': contract = contract[2:]
    if len(contract) != 40: raise ValueError('invalid address')
    funsig = 'unwrap(uint256)'
    method = keccak256(funsig.encode())[:4]
    data = method + n2b(amount, 32)
    fields = {}
    fields['nonce'] = nonce
    fields['gasprice'] = 1
    fields['gaslimit'] = 100000
    fields['to'] = contract
    fields['data'] = data
    fields['v'] = n2b(27)
    return txn_encode(fields);

# generates a token transfer transaction
def txn_transfer(nonce, contract, to, amount):
    if contract[:2] == '0x': contract = contract[2:]
    if len(contract) != 40: raise ValueError('invalid address')
    if to[:2] == '0x': to = to[2:]
    if len(to) != 40: raise ValueError('invalid address')
    funsig = 'transfer(address,uint256)'
    method = keccak256(funsig.encode())[:4]
    data = method + n2b(0, 12) + h2b(to) + n2b(amount, 32)
    fields = {}
    fields['nonce'] = nonce
    fields['gasprice'] = 1
    fields['gaslimit'] = 100000
    fields['to'] = contract
    fields['data'] = data
    fields['v'] = n2b(27)
    return txn_encode(fields);

# generates a native transfer transaction
def txn_nativetransfer(nonce, to, amount):
    if to[:2] == '0x': to = to[2:]
    if len(to) != 40: raise ValueError('invalid address')
    fields = {}
    fields['nonce'] = nonce
    fields['gasprice'] = 1
    fields['gaslimit'] = 100000
    fields['to'] = to
    fields['value'] = amount
    fields['v'] = n2b(27)
    return txn_encode(fields);

def main():
    if not len(sys.argv) in [4, 5, 6]:
        print('usage: python3 ' + sys.argv[0] + ' address <from> <nonce>                    -- contract address')
        print('usage: python3 ' + sys.argv[0] + ' create <nonce> <binfile>                  -- contract creation transaction')
#        print('usage: python3 ' + sys.argv[0] + ' create <nonce> <binfile> <supply>         -- contract creation w/ supply transaction')
        print('usage: python3 ' + sys.argv[0] + ' wrap <nonce> <contract> <amount>          -- token wrap transaction')
        print('usage: python3 ' + sys.argv[0] + ' unwrap <nonce> <contract> <amount>        -- token unwrap transaction')
        print('usage: python3 ' + sys.argv[0] + ' transfer <nonce> <contract> <to> <amount> -- token transfer transaction')
        print('usage: python3 ' + sys.argv[0] + ' transfer <nonce> <to> <amount>            -- native transfer transaction')
        return
    if len(sys.argv) == 4 and sys.argv[1] == 'create': print(b2h(txn_contract(int(sys.argv[2]), sys.argv[3])))
#    if len(sys.argv) == 5 and sys.argv[1] == 'create': print(b2h(txn_contract(int(sys.argv[2]), sys.argv[3], int(sys.argv[4]))))
    if len(sys.argv) == 4 and sys.argv[1] == 'address': print(b2h(txn_address(sys.argv[2], int(sys.argv[3]))))
    if len(sys.argv) == 5 and sys.argv[1] == 'wrap': print(b2h(txn_wrap(int(sys.argv[2]), sys.argv[3], int(sys.argv[4]))))
    if len(sys.argv) == 5 and sys.argv[1] == 'unwrap': print(b2h(txn_unwrap(int(sys.argv[2]), sys.argv[3], int(sys.argv[4]))))
    if len(sys.argv) == 6 and sys.argv[1] == 'transfer': print(b2h(txn_transfer(int(sys.argv[2]), sys.argv[3], sys.argv[4], int(sys.argv[5]))))
    if len(sys.argv) == 5 and sys.argv[1] == 'transfer': print(b2h(txn_nativetransfer(int(sys.argv[2]), sys.argv[3], int(sys.argv[4]))))

if __name__ == '__main__': main()
