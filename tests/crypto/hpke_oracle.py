#!/usr/bin/env python3
# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
"""The oracle of crypto::hpke's modes beyond base, which Go's crypto/hpke does
not have: RFC 9180's DHKEM(X25519, HKDF-SHA256) with AuthEncap and its key
schedule in the four modes, written by hand over Python's standard library
(hashlib, hmac) and RFC 7748's X25519. Everything comes from a line of
arguments, in hex:

    hpke_oracle.py <mode> <kdf 1|2|3> <aead 1|2|3|65535> <ikmR> <ikmE> <ikmS|-> <psk|-> <psk_id|-> <info>

and it prints, one per line: pkRm, enc, key, base_nonce, exporter_secret, and
Export("context", 32)."""
import hashlib
import hmac
import sys

P = 2**255 - 19
A24 = 121665


def x25519(k, u):
    k = bytearray(k)
    k[0] &= 248
    k[31] &= 127
    k[31] |= 64
    k = int.from_bytes(k, 'little')
    u = int.from_bytes(u, 'little') & ((1 << 255) - 1)
    x1, x2, z2, x3, z3, swap = u, 1, 0, u, 1, 0
    for t in reversed(range(255)):
        kt = (k >> t) & 1
        swap ^= kt
        if swap:
            x2, x3, z2, z3 = x3, x2, z3, z2
        swap = kt
        a, b = (x2 + z2) % P, (x2 - z2) % P
        aa, bb = a * a % P, b * b % P
        e = (aa - bb) % P
        c, d = (x3 + z3) % P, (x3 - z3) % P
        da, cb = d * a % P, c * b % P
        x3 = (da + cb) ** 2 % P
        z3 = x1 * (da - cb) ** 2 % P
        x2 = aa * bb % P
        z2 = e * (aa + A24 * e) % P
    if swap:
        x2, z2 = x3, z3
    return (x2 * pow(z2, P - 2, P) % P).to_bytes(32, 'little')


BASE = (9).to_bytes(32, 'little')
HASHES = {1: hashlib.sha256, 2: hashlib.sha384, 3: hashlib.sha512}


def extract(h, salt, ikm):
    if not salt:
        salt = bytes(h().digest_size)
    return hmac.new(salt, ikm, h).digest()


def expand(h, prk, info, n):
    out, t, i = b'', b'', 1
    while len(out) < n:
        t = hmac.new(prk, t + info + bytes([i]), h).digest()
        out += t
        i += 1
    return out[:n]


def labeled_extract(h, suite, salt, label, ikm):
    return extract(h, salt, b'HPKE-v1' + suite + label + ikm)


def labeled_expand(h, suite, prk, label, info, n):
    return expand(h, prk, n.to_bytes(2, 'big') + b'HPKE-v1' + suite + label + info, n)


KEM = b'KEM' + (0x20).to_bytes(2, 'big')


def derive(ikm):
    prk = labeled_extract(hashlib.sha256, KEM, b'', b'dkp_prk', ikm)
    sk = labeled_expand(hashlib.sha256, KEM, prk, b'sk', b'', 32)
    return sk, x25519(sk, BASE)


def main():
    a = sys.argv[1:]
    mode, kdf, aead = int(a[0]), int(a[1]), int(a[2])
    h = lambda x: b'' if x == '-' else bytes.fromhex(x)
    skR, pkR = derive(h(a[3]))
    skE, pkE = derive(h(a[4]))
    dh = x25519(skE, pkR)
    ctx = pkE + pkR
    if a[5] != '-':
        skS, pkS = derive(h(a[5]))
        dh += x25519(skS, pkR)
        ctx += pkS
    eae = labeled_extract(hashlib.sha256, KEM, b'', b'eae_prk', dh)
    shared = labeled_expand(hashlib.sha256, KEM, eae, b'shared_secret', ctx, 32)
    psk, psk_id, info = h(a[6]), h(a[7]), h(a[8])
    H = HASHES[kdf]
    suite = b'HPKE' + (0x20).to_bytes(2, 'big') + kdf.to_bytes(2, 'big') + aead.to_bytes(2, 'big')
    ksc = bytes([mode]) + labeled_extract(H, suite, b'', b'psk_id_hash', psk_id) + labeled_extract(H, suite, b'', b'info_hash', info)
    secret = labeled_extract(H, suite, shared, b'secret', psk)
    nk = {1: 16, 2: 32, 3: 32, 65535: 0}[aead]
    key = labeled_expand(H, suite, secret, b'key', ksc, nk) if nk else b''
    nonce = labeled_expand(H, suite, secret, b'base_nonce', ksc, 12)
    exp = labeled_expand(H, suite, secret, b'exp', ksc, H().digest_size)
    export = labeled_expand(H, suite, exp, b'sec', b'context', 32)
    for v in (pkR, pkE, key, nonce, exp, export):
        print(v.hex() if v else '-')


main()
