#!/usr/bin/env python3
"""GameMaker 2024.14 runtime RNG (Spelunky Classic HD 1.2.2's runner), bit-exact model.

    tools/gmrand.py --check build/trace/rng_probe.bin     compare with a probe (scripts/hd_trace.sh --rng-probe)
    tools/gmrand.py <seed> [n]                            the first n random(1) values for a seed

From the runner (Android libyoyo.so of the same runtime, armeabi-v7a, with symbols; checked against the Linux
arm64 runner's draws by --check):

State: 16 x u32 (WELL512a) and an index i (InitRandom(unsigned), YYRandom(), fYYRandom(double), iYYRandom()).

Seeding, random_set_seed(s) -> InitRandom((u32)s): g_nRandSeed = s (random_get_seed() returns it, as u32:
-1 -> 4294967295); x0 = s; x(k+1) = ((x(k) * 0x343FD + 0x269EC3) mod 2^32) >> 16 (the MSVC rand() LCG, the
16-bit result fed back); state[k] = x(k+1) for k = 0..15, so every word is < 2^16; i = 0.

Step (WELL512a, Panneton / L'Ecuyer / Matsumoto), returning u32:
    a = S[i]; c = S[(i+13)&15]; b = a ^ c ^ (a<<16) ^ (c<<15)
    c = S[(i+9)&15]; c ^= c>>11
    a = S[i] = b ^ c; d = a ^ ((a<<5) & 0xDA442D24)
    i = (i+15)&15; a = S[i]
    S[i] = a ^ b ^ d ^ (a<<2) ^ (b<<18) ^ (c<<28)   -> the output
random(n) (YYGML_random / F_Random): fYYRandom(1.0) * n, fYYRandom(1.0) = (double)u * 2^-32 (exact), then one
    double multiplication by n (round to nearest). So random(n) = fl(u * n) / 2^32 exactly (scaling by 2^-32 is
    exact for normal results).
irandom(n) (YYGML_irandom / F_IRandom): m = n + 1 (n >= 0; n - 1 for negative n, result negated); two draws
    lo = u1, hi = u2 & 0x7FFFFFFF; result = ((hi << 32) | lo) mod |m|  (signed 64-bit).
floor(random(n)) for integer n (rand(a, b) = floor(random(b - a + 1)) + a): u * n is exact in a double while
    u * n < 2^53, i.e. for n <= 2^21; then floor(random(n)) = (u * n) >> 32 in integers, exactly. For larger n the
    double product rounds and the integer form can differ.
"""
import struct
import sys

M = 0xFFFFFFFF


class GMRand:
    def __init__(self, seed=0):
        self.seed(seed)

    def seed(self, s):
        s &= M
        self.seed_value = s
        x, st = s, []
        for _ in range(16):
            x = ((x * 0x343FD + 0x269EC3) & M) >> 16
            st.append(x)
        self.S, self.i = st, 0

    def next_u32(self):
        S, i = self.S, self.i
        a = S[i]
        c = S[(i + 13) & 15]
        b = (a ^ c ^ (a << 16) ^ (c << 15)) & M
        c = S[(i + 9) & 15]
        c = (c ^ (c >> 11)) & M
        a = S[i] = b ^ c
        d = (a ^ ((a << 5) & 0xDA442D24)) & M
        i = (i + 15) & 15
        a = S[i]
        S[i] = (a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28)) & M
        self.i = i
        return S[i]

    def random(self, n=1.0):
        return float(self.next_u32()) * 2.0 ** -32 * n

    def irandom(self, n):
        n = int(n)
        m = n + 1 if n >= 0 else n - 1
        lo = self.next_u32()
        hi = self.next_u32() & 0x7FFFFFFF
        r = ((hi << 32) | lo) % abs(m)
        return r if n >= 0 else -r

    def rand(self, a, b):
        """the game's scripts/rand: floor(random(b - a + 1)) + a, in integers (exact for b - a + 1 <= 2^21)"""
        return ((self.next_u32() * (b - a + 1)) >> 32) + a


def check(path, n=2000):
    d = open(path, 'rb').read()
    v = struct.unpack('<%dd' % (len(d) // 8), d)
    rec = 2 + 5 * n
    NS = [3, 7, 13, 0.7, 100.5, 1000000.3]   # tools/tracer.py PROBE_NS / PROBE_IS
    IS = [1, 10, 99, -5]
    bad = 0
    for k in range(len(v) // rec):
        b = v[k * rec:(k + 1) * rec]
        seed = int(b[0])
        g = GMRand(seed)
        got = [float(g.seed_value)]
        got += [g.random(1) for _ in range(n)]
        g.seed(seed)
        got += [g.random(4294967296) for _ in range(n)]
        g.seed(seed)
        got += [float(g.irandom(2147483647)) for _ in range(n)]
        g.seed(seed)
        got += [g.random(NS[i % 6]) for i in range(n)]
        g.seed(seed)
        got += [float(g.irandom(IS[i % 4])) for i in range(n)]
        want = list(b[1:])
        diff = sum(1 for x, y in zip(got, want) if struct.pack('<d', x) != struct.pack('<d', y))
        bad += diff
        print(f'seed {seed}: random_get_seed {want[0]!r}, {5 * n} draws, {diff} differ')
    print('PASS' if bad == 0 else f'FAIL ({bad})')
    return bad == 0


if __name__ == '__main__':
    a = sys.argv[1:]
    if a and a[0] == '--check':
        sys.exit(0 if check(a[1]) else 1)
    elif a:
        g = GMRand(int(a[0]))
        for _ in range(int(a[1]) if len(a) > 1 else 10):
            print(repr(g.random(1)))
    else:
        sys.exit(__doc__)
