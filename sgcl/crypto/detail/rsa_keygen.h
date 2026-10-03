//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "rsa_math.h"
#include "../random.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// The primes of a new RSA key and what is derived from them, every value a
// secret handled with the constant-time arithmetic of rsa_math.h.
//
// A prime is drawn as FIPS 186-5 draws it: fresh random bits for
// every candidate, the top two set (so that the product of two has exactly
// the bits asked for) and the lowest; trial division by the odd primes
// below 2048 and by e (a candidate p with p = 1 mod e is refused: e must
// be prime to p - 1); then Miller-Rabin with random bases. What a refused
// candidate shows by the time it took does not matter: it is thrown away.
// The trial division of the one that is kept runs in the same time as any
// other (products and a Barrett reduction, no division instruction, whose
// time depends on its operands on some processors). Miller-Rabin shows how
// many times 2 divides p - 1 (the count of its squarings), a few low bits
// of p on average, as other implementations do.
//
// From p and q: dP = e^-1 mod (p - 1) and dQ = e^-1 mod (q - 1) without an
// extended Euclid over a secret (whose steps depend on it): for the prime
// e and an m that e does not divide, k = -m^-1 mod e is a small number
// (Fermat's inverse modulo e, in words of 64 bits), and 1 + k m is then a
// multiple of e whose quotient (an exact division by multiplication with
// e^-1 mod 2^64) is the inverse; d = e^-1 mod (p - 1)(q - 1) the same way;
// qInv = q^(p - 2) mod p.
namespace sgcl::crypto::detail::rsa_keygen {
    using bn::word;
    using bn::wide;

    // The odd primes below 2048
    consteval size_t count_small_primes() {
        size_t c = 0;
        for (unsigned n = 3; n < 2048; n += 2) {
            bool prime = true;
            for (unsigned d = 3; d * d <= n; d += 2) {
                if (n % d == 0) {
                    prime = false;
                    break;
                }
            }
            c += prime;
        }
        return c;
    }

    inline constexpr size_t small_prime_count = count_small_primes();

    consteval std::array<uint32_t, small_prime_count> make_small_primes() {
        std::array<uint32_t, small_prime_count> a{};
        size_t c = 0;
        for (unsigned n = 3; n < 2048; n += 2) {
            bool prime = true;
            for (unsigned d = 3; d * d <= n; d += 2) {
                if (n % d == 0) {
                    prime = false;
                    break;
                }
            }
            if (prime) {
                a[c++] = n;
            }
        }
        return a;
    }

    inline constexpr std::array<uint32_t, small_prime_count> small_primes = make_small_primes();

    // x mod s for a small public s (odd, above 2) and any x of 64 bits:
    // Barrett's quotient from mu = floor(2^64 / s), then one correction
    // through a mask; no division instruction
    struct SmallModulus {
        uint64_t s = 0;
        uint64_t mu = 0;

        explicit SmallModulus(uint64_t v = 3) noexcept
        : s(v), mu(uint64_t(~uint64_t(0) / v)) {
        }

        uint64_t reduce(uint64_t x) const noexcept {
            uint64_t q = uint64_t((wide(x) * mu) >> 64);
            uint64_t r = x - q * s;
            uint64_t over = ct_bit_mask(uint64_t(((wide(r) - s) >> 64) & 1) ^ 1);
            return r - (s & over);
        }
    };

    // The residues of candidates of k words modulo the small primes and e:
    // each candidate's 32-bit pieces times 2^(32 i) mod s, summed (below
    // 2^58 for keys up to 16384 bits) and reduced once
    class Sieve {
    public:
        Sieve(size_t k, uint64_t e) noexcept
        : _pieces(2 * k), _weights((small_prime_count + 1) * 2 * k) {
            for (size_t j = 0; j <= small_prime_count; ++j) {
                uint64_t s = j < small_prime_count ? small_primes[j] : e;
                _mods[j] = SmallModulus(s);
                uint64_t w = 1;
                for (size_t i = 0; i < _pieces; ++i) {
                    _weights[j * _pieces + i] = uint32_t(w);
                    w = (w << 32) % s;   // public
                }
            }
        }

        // Whether the candidate has no small factor and is not 1 mod e;
        // its residue mod e in *mod_e
        bool passes(const word* x, uint64_t* mod_e) const noexcept {
            bool ok = true;
            for (size_t j = 0; j <= small_prime_count; ++j) {
                const uint32_t* w = _weights.data() + j * _pieces;
                uint64_t sum = 0;
                for (size_t i = 0; i < _pieces; ++i) {
                    uint64_t piece = uint32_t(x[i / 2] >> (32 * (i % 2)));
                    sum += piece * w[i];
                }
                uint64_t r = _mods[j].reduce(sum);
                if (j < small_prime_count) {
                    ok &= r != 0;   // a refusal is public
                } else {
                    ok &= r != 1;
                    *mod_e = r;
                }
            }
            return ok;
        }

        const SmallModulus& e() const noexcept {
            return _mods[small_prime_count];
        }

    private:
        size_t _pieces;
        std::vector<uint32_t> _weights;   // public: powers of two modulo public numbers
        std::array<SmallModulus, small_prime_count + 1> _mods;
    };

    // Miller-Rabin rounds for a candidate that passed the sieve: a
    // composite passes one with probability at most 1/4 whatever it is, a
    // random candidate of a thousand bits and more with far less
    // (Damgård, Landrock and Pomerance, 1993)
    inline constexpr int miller_rabin_rounds = 16;

    // Whether w (odd, k words, its top two bits set) passes Miller-Rabin
    // with random bases. scratch: many words, see the caller
    template<class Release>
    bool probably_prime(const word* w, size_t k) noexcept {
        using namespace bn;
        SecretWords<Release> s(8 * k + 2 + pow_scratch(k) + reduce_scratch(k));
        word* rr = s.data();
        word* rrr = rr + k;
        word* m = rrr + k;      // (w - 1) / 2^a
        word* b = m + k;
        word* z = b + k;
        word* one = z + k;
        word* minus_one = one + k;
        word* zero_w = minus_one + k;
        word* scratch = zero_w + k + 2;
        word m0inv = mont_m0inv(w[0]);
        mont_constants(rr, rrr, w, k, m0inv, scratch);
        Modulus mod{w, rr, rrr, m0inv, k};
        // w - 1 = 2^a m
        copy(m, w, k);
        m[0] ^= 1;
        size_t a = 0;
        while (((m[a / 64] >> (a % 64)) & 1) == 0) {
            ++a;
        }
        for (size_t i = 0; i < k; ++i) {
            size_t from = i + a / 64;
            size_t sh = a % 64;
            word lo = from < k ? m[from] >> sh : 0;
            word hi = sh != 0 && from + 1 < k ? m[from + 1] << (64 - sh) : 0;
            m[i] = lo | hi;
        }
        // 1 and -1 in the form
        zero(zero_w, k);
        zero(b, k);
        b[0] = 1;
        to_mont(one, b, mod, scratch);
        mod_sub(minus_one, zero_w, one, w, k);
        size_t bits = bit_length(w, k);
        for (int round = 0; round < miller_rabin_rounds; ++round) {
            // a base in [2, w - 2]: random below 2^(bits - 1)
            for (;;) {
                random::fill(slice<byte>(reinterpret_cast<byte*>(b), k * sizeof(word)));
                for (size_t i = 0; i < k; ++i) {
                    size_t lo = 64 * i;
                    if (lo >= bits - 1) {
                        b[i] = 0;
                    } else if (bits - 1 - lo < 64) {
                        b[i] &= (word(1) << (bits - 1 - lo)) - 1;
                    }
                }
                word big = 0;
                for (size_t i = 1; i < k; ++i) {
                    big |= b[i];
                }
                if (big != 0 || b[0] >= 2) {
                    break;
                }
            }
            to_mont(z, b, mod, scratch);
            mont_pow(z, z, m, 64 * k, mod, scratch);
            if (equal_mask(z, one, k) != 0 || equal_mask(z, minus_one, k) != 0) {
                continue;
            }
            bool witness = true;
            for (size_t j = 1; j < a; ++j) {
                mont_sqr(z, z, mod, scratch);
                if (equal_mask(z, minus_one, k) != 0) {
                    witness = false;
                    break;
                }
                if (equal_mask(z, one, k) != 0) {
                    break;
                }
            }
            if (witness) {
                return false;
            }
        }
        return true;
    }

    // A random prime of bits bits (k = ceil(bits / 64) words) into p, with
    // p mod e in *mod_e
    template<class Release>
    void random_prime(word* p, size_t bits, const Sieve& sieve, uint64_t* mod_e) noexcept {
        size_t k = (bits + 63) / 64;
        for (;;) {
            random::fill(slice<byte>(reinterpret_cast<byte*>(p), k * sizeof(word)));
            size_t top = bits - 64 * (k - 1);   // bits in the top word, 1..64
            if (top < 64) {
                p[k - 1] &= (word(1) << top) - 1;
            }
            // the top two bits and the lowest
            p[k - 1] |= word(1) << (top - 1);
            if (top >= 2) {
                p[k - 1] |= word(1) << (top - 2);
            } else {
                p[k - 2] |= word(1) << 63;
            }
            p[0] |= 1;
            if (!sieve.passes(p, mod_e)) {
                continue;
            }
            if (probably_prime<Release>(p, k)) {
                return;
            }
        }
    }

    // a^b mod e for small numbers (a < e < 2^32), the exponent public
    inline uint64_t small_pow(uint64_t a, uint64_t b, const SmallModulus& e) noexcept {
        uint64_t r = 1;
        for (int i = 63; i >= 0; --i) {
            r = e.reduce(r * r);
            if ((b >> i) & 1) {
                r = e.reduce(r * a);
            }
        }
        return r;
    }

    // e^-1 mod m for m of km words, e prime and m mod e = rm (not 0):
    // k = (e - rm)^(e - 2) mod e = -m^-1 mod e, then (1 + k m) / e, which
    // is exact; below m. out: km words. scratch: km + 1 words
    inline void inverse_of_e(word* out, const word* m, size_t km, uint64_t rm, const SmallModulus& e, word* scratch) noexcept {
        uint64_t k = small_pow(e.s - rm, e.s - 2, e);
        word* t = scratch;   // km + 1 words
        bn::mul_word(t, m, km, k);
        // + 1: k m + 1 < e m, no carry out of the top word
        word carry = 1;
        for (size_t i = 0; i <= km; ++i) {
            wide x = wide(t[i]) + carry;
            t[i] = word(x);
            carry = word(x >> 64);
        }
        // exact division by the odd e: each quotient word is the word less
        // the borrow times e^-1 mod 2^64, the high half of its product
        // with e the next borrow
        word inv = 1;
        for (int i = 0; i < 7; ++i) {
            inv *= 2 - e.s * inv;
        }
        word borrow = 0;
        for (size_t i = 0; i <= km; ++i) {
            word a = t[i];
            word below = word(((wide(a) - borrow) >> 64) & 1);
            word v = a - borrow;
            word q = v * inv;
            if (i < km) {
                out[i] = q;
            }
            borrow = word((wide(q) * e.s) >> 64) + below;
        }
    }

    // a shifted right by one bit where mask is all ones, left as it is
    // where it is zero (k words)
    inline void shift_right_masked(word* a, size_t k, word mask) noexcept {
        for (size_t i = 0; i < k; ++i) {
            word hi = i + 1 < k ? a[i + 1] : 0;
            word shifted = (a[i] >> 1) | (hi << 63);
            a[i] = (shifted & mask) | (a[i] & ~mask);
        }
    }

    // gcd(a, b) of two numbers of k words, not both zero, into g (k words),
    // in a time that depends on k alone (Stein's algorithm with every step
    // done under masks, 128 k rounds: each takes a bit off u or v, or
    // both). a and b are secrets (p - 1 and q - 1). scratch: 2k words
    inline void gcd_consttime(word* g, const word* a, const word* b, size_t k, word* scratch) noexcept {
        word* u = scratch;
        word* v = scratch + k;
        bn::copy(u, a, k);
        bn::copy(v, b, k);
        word shift = 0;
        for (size_t round = 0; round < 128 * k; ++round) {
            // both odd: the larger less the smaller, which is even
            word both_odd = ct_bit_mask(u[0] & v[0] & 1);
            word u_less = bn::less_mask(u, v, k);
            // g as the difference, taken from the larger: v - u where u < v
            bn::sub(g, v, u, k);
            bn::select(v, both_odd & u_less, g, v, k);
            bn::sub(g, u, v, k);
            bn::select(u, both_odd & ~u_less, g, u, k);
            // halve what is even; both even: a common factor of 2
            word u_even = ct_bit_mask((u[0] & 1) ^ 1);
            word v_even = ct_bit_mask((v[0] & 1) ^ 1);
            word u_zero = bn::zero_mask(u, k);
            word v_zero = bn::zero_mask(v, k);
            shift += (u_even & v_even & ~u_zero & ~v_zero) & 1;
            shift_right_masked(u, k, u_even & ~u_zero);
            shift_right_masked(v, k, v_even & ~v_zero);
        }
        // one of the two is zero, the other the odd part of the gcd
        for (size_t i = 0; i < k; ++i) {
            g[i] = u[i] | v[i];
        }
        // times 2^shift, in 64 k steps whatever shift is
        for (size_t i = 0; i < 64 * k; ++i) {
            word on = ct_bit_mask(word(i < shift));
            word carry = 0;
            for (size_t j = 0; j < k; ++j) {
                word shifted = (g[j] << 1) | carry;
                carry = g[j] >> 63;
                g[j] = (shifted & on) | (g[j] & ~on);
            }
        }
        bn::zero(scratch, 2 * k);
    }

    // q = a / d, exactly or not, for a of ka words and d (nonzero) of kd
    // words, bit by bit from the top in 64 ka steps: the remainder shifted
    // in and d taken off under a mask. q: ka words. scratch: 2 (kd + 1)
    inline void div_consttime(word* q, const word* a, size_t ka, const word* d, size_t kd, word* scratch) noexcept {
        size_t kr = kd + 1;
        word* r = scratch;
        word* t = scratch + kr;
        bn::zero(r, kr);
        bn::zero(q, ka);
        bn::SecretWords<> dd(kr);   // zero when made, zeroed when it goes
        bn::copy(dd.data(), d, kd);
        for (size_t i = 64 * ka; i-- > 0;) {
            word bit = (a[i / 64] >> (i % 64)) & 1;
            word carry = bit;
            for (size_t j = 0; j < kr; ++j) {
                word next = r[j] >> 63;
                r[j] = (r[j] << 1) | carry;
                carry = next;
            }
            word below = bn::less_mask(r, dd.data(), kr);
            bn::sub(t, r, dd.data(), kr);
            bn::select(r, ~below, t, r, kr);
            q[i / 64] |= (~below & 1) << (i % 64);
        }
        bn::zero(scratch, 2 * kr);
    }

    // a mod e for a of k words (e below 2^32), in 32-bit steps from the top
    inline uint64_t mod_small(const word* a, size_t k, const SmallModulus& e) noexcept {
        uint64_t r = 0;
        for (size_t i = k; i-- > 0;) {
            r = e.reduce((r << 32) | (a[i] >> 32));
            r = e.reduce((r << 32) | (a[i] & 0xffffffffu));
        }
        return r;
    }
}
