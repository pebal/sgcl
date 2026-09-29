//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "ec_field.h"
#include "../secure_zero.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <utility>

// The arithmetic of RSA: numbers of any size as arrays of 64-bit words,
// least significant first, their width (the count of words) fixed by the
// modulus they belong to and never by their value. Montgomery
// multiplication modulo an odd number of k words, the modulus known only at
// run time (the n, p and q of a key), with R = 2^(64k).
//
// Constant time. Every function here but the ones named _vartime runs the
// same instructions and touches the same memory whatever the words hold:
// the one conditional step of an operation (the final subtraction of the
// modulus, a doubling's reduction) is a select through a mask made from a
// carry and passed through ct_barrier. What may depend on a number is its
// width, and the bit length of a modulus, which a key's encoding shows
// anyway. Exponentiation by a secret exponent goes four bits at a time,
// each window's power read from a table by reading every entry and keeping
// the one whose index matches through a mask. The _vartime functions are
// for values that are public (a public exponent, a signature, a
// ciphertext) or random and blinded (the inverse of the blinding factor).
namespace sgcl::crypto::detail::bn {
    using word = uint64_t;
    using wide = unsigned __int128;

    // --- storage -----------------------------------------------------------

    // How the words of a secret go back to the system after they were
    // zeroed: operator delete. A policy, so that a test can put a probe in
    // between (tests/crypto/rsa.cpp checks that every block a key frees is
    // zero)
    struct OperatorDelete {
        static void release(void* p, size_t) noexcept {
            ::operator delete(p);
        }
    };

    // n words on the heap, zero when made, zeroed with stores the compiler
    // cannot drop when they go; move-only, a move leaves the source empty.
    // Every word of a private key and every scratch word of an operation on
    // one lives in one of these
    template<class Release = OperatorDelete>
    class SecretWords {
    public:
        SecretWords() noexcept = default;

        explicit SecretWords(size_t n)
        : _p(n != 0 ? static_cast<word*>(::operator new(n * sizeof(word))) : nullptr), _n(n) {
            if (_p) {
                std::memset(_p, 0, n * sizeof(word));
            }
        }

        SecretWords(const SecretWords&) = delete;
        SecretWords& operator=(const SecretWords&) = delete;

        SecretWords(SecretWords&& o) noexcept
        : _p(std::exchange(o._p, nullptr)), _n(std::exchange(o._n, 0)) {
        }

        SecretWords& operator=(SecretWords&& o) noexcept {
            if (this != &o) {
                _free();
                _p = std::exchange(o._p, nullptr);
                _n = std::exchange(o._n, 0);
            }
            return *this;
        }

        ~SecretWords() {
            _free();
        }

        word* data() noexcept {
            return _p;
        }

        const word* data() const noexcept {
            return _p;
        }

        size_t size() const noexcept {
            return _n;
        }

        explicit operator bool() const noexcept {
            return _p != nullptr;
        }

    private:
        word* _p = nullptr;
        size_t _n = 0;

        void _free() noexcept {
            if (_p) {
                secure_zero(_p, _n * sizeof(word));
                Release::release(_p, _n * sizeof(word));
                _p = nullptr;
                _n = 0;
            }
        }
    };

    // --- words -------------------------------------------------------------

    inline size_t words_for_bytes(size_t n) noexcept {
        return (n + 7) / 8;
    }

    inline void zero(word* r, size_t k) noexcept {
        for (size_t i = 0; i < k; ++i) {
            r[i] = 0;
        }
    }

    inline void copy(word* r, const word* a, size_t k) noexcept {
        for (size_t i = 0; i < k; ++i) {
            r[i] = a[i];
        }
    }

    // r = a + b over k words; the carry out
    inline word add(word* r, const word* a, const word* b, size_t k) noexcept {
        word carry = 0;
        for (size_t i = 0; i < k; ++i) {
            wide t = wide(a[i]) + b[i] + carry;
            r[i] = word(t);
            carry = word(t >> 64);
        }
        return carry;
    }

    // r = a - b over k words; the borrow out
    inline word sub(word* r, const word* a, const word* b, size_t k) noexcept {
        word borrow = 0;
        for (size_t i = 0; i < k; ++i) {
            wide t = wide(a[i]) - b[i] - borrow;
            r[i] = word(t);
            borrow = word(t >> 64) & 1;
        }
        return borrow;
    }

    // r = mask ? a : b, word by word (mask all ones or zero)
    inline void select(word* r, word mask, const word* a, const word* b, size_t k) noexcept {
        for (size_t i = 0; i < k; ++i) {
            r[i] = (a[i] & mask) | (b[i] & ~mask);
        }
    }

    // All ones when a < b
    inline word less_mask(const word* a, const word* b, size_t k) noexcept {
        word borrow = 0;
        for (size_t i = 0; i < k; ++i) {
            wide t = wide(a[i]) - b[i] - borrow;
            borrow = word(t >> 64) & 1;
        }
        return ct_bit_mask(borrow);
    }

    // All ones when a == b
    inline word equal_mask(const word* a, const word* b, size_t k) noexcept {
        word d = 0;
        for (size_t i = 0; i < k; ++i) {
            d |= a[i] ^ b[i];
        }
        return ct_zero_mask(d);
    }

    // All ones when a is 0
    inline word zero_mask(const word* a, size_t k) noexcept {
        word d = 0;
        for (size_t i = 0; i < k; ++i) {
            d |= a[i];
        }
        return ct_zero_mask(d);
    }

    // All ones when a is 1
    inline word one_mask(const word* a, size_t k) noexcept {
        word d = a[0] ^ 1;
        for (size_t i = 1; i < k; ++i) {
            d |= a[i];
        }
        return ct_zero_mask(d);
    }

    // The bit length of a public number (a modulus, an exponent)
    inline size_t bit_length(const word* a, size_t k) noexcept {
        for (size_t i = k; i-- > 0;) {
            if (a[i] != 0) {
                return 64 * i + 64 - size_t(__builtin_clzll(a[i]));
            }
        }
        return 0;
    }

    // n big-endian bytes into k words (n <= 8k), zeros above them
    inline void from_be(word* r, size_t k, const unsigned char* p, size_t n) noexcept {
        zero(r, k);
        for (size_t i = 0; i < n; ++i) {
            size_t bit = 8 * (n - 1 - i);
            r[bit / 64] |= word(p[i]) << (bit % 64);
        }
    }

    // The low n bytes of k words, big-endian (zeros in front when n > 8k)
    inline void to_be(unsigned char* p, size_t n, const word* a, size_t k) noexcept {
        for (size_t i = 0; i < n; ++i) {
            size_t bit = 8 * (n - 1 - i);
            p[i] = bit / 64 < k ? static_cast<unsigned char>(a[bit / 64] >> (bit % 64)) : 0;
        }
    }

    // r = a * b, ka + kb words; r aliases neither
    inline void mul(word* r, const word* a, size_t ka, const word* b, size_t kb) noexcept {
        zero(r, ka + kb);
        for (size_t i = 0; i < kb; ++i) {
            word c = 0;
            word bi = b[i];
            for (size_t j = 0; j < ka; ++j) {
                wide x = wide(a[j]) * bi + r[i + j] + c;
                r[i + j] = word(x);
                c = word(x >> 64);
            }
            r[i + ka] = c;
        }
    }

    // r = a * w for one word w, k + 1 words
    inline void mul_word(word* r, const word* a, size_t k, word w) noexcept {
        word c = 0;
        for (size_t i = 0; i < k; ++i) {
            wide x = wide(a[i]) * w + c;
            r[i] = word(x);
            c = word(x >> 64);
        }
        r[k] = c;
    }

    // x mod m for any m > 0 of k words, x of kx words: one bit of x at a
    // time from the top, r doubled with the bit and reduced once (r < m
    // keeps 2r + 1 < 2m). For the checks of a key, whose moduli p - 1 and
    // q - 1 are even and have no Montgomery form: 64 kx steps of k words
    inline void reduce(word* r, const word* x, size_t kx, const word* m, size_t k, word* t) noexcept {
        zero(r, k);
        for (size_t i = 64 * kx; i-- > 0;) {
            word in = (x[i / 64] >> (i % 64)) & 1;
            word carry = 0;
            for (size_t j = 0; j < k; ++j) {
                word w = r[j];
                r[j] = w << 1 | in;
                in = w >> 63;
            }
            carry = in;
            word borrow = sub(t, r, m, k);
            // subtract when the doubled r (with its carry) is not below m
            word keep = ct_bit_mask(borrow & ~carry & 1);
            select(r, keep, r, t, k);
        }
    }

    // --- Montgomery ----------------------------------------------------------

    // A modulus m, odd, of k words, with what Montgomery multiplication
    // needs: -m^-1 mod 2^64, R^2 mod m (what takes a number into the form)
    // and R^3 mod m (what takes a number of 2k words, below m R, into the
    // form through one reduction). A view: the words live in the key
    struct Modulus {
        const word* m = nullptr;
        const word* rr = nullptr;
        const word* rrr = nullptr;
        word m0inv = 0;
        size_t k = 0;
    };

    // -m0^-1 mod 2^64 for an odd m0, by Newton's iteration (each step
    // doubles the bits that are right)
    inline word mont_m0inv(word m0) noexcept {
        word inv = 1;
        for (int i = 0; i < 7; ++i) {
            inv *= 2 - m0 * inv;
        }
        return 0 - inv;
    }

    // Words of scratch the functions below take, for a modulus of k words
    inline size_t mul_scratch(size_t k) noexcept {
        return 3 * k + 2;
    }

    // r = t - m when t (k words and a top word hi, t < 2m) is at least m,
    // r = t otherwise; r may be t
    inline void reduce_once(word* r, const word* t, word hi, const word* m, size_t k, word* u) noexcept {
        word borrow = sub(u, t, m, k);
        word keep = ct_bit_mask(borrow & ~hi & 1);
        select(r, keep, t, u, k);
    }

    // A sum of products of words in three words, what the product scanning
    // below adds each column into
    struct Accumulator {
        word a0 = 0;
        word a1 = 0;
        word a2 = 0;

        void add(word x, word y) noexcept {
            wide p = wide(x) * y;
            wide s = (wide(a1) << 64 | a0) + p;
            a2 += word(s < p);
            a0 = word(s);
            a1 = word(s >> 64);
        }

        // 2^i times o added (i = 0 or 1), o emptied
        void take(Accumulator& o, unsigned i) noexcept {
            word b0 = o.a0 << i;
            word b1 = o.a1 << i | (i != 0 ? o.a0 >> 63 : 0);
            word b2 = o.a2 << i | (i != 0 ? o.a1 >> 63 : 0);
            wide b = wide(b1) << 64 | b0;
            wide s = (wide(a1) << 64 | a0) + b;
            a2 += b2 + word(s < b);
            a0 = word(s);
            a1 = word(s >> 64);
            o = Accumulator();
        }

        // The lowest word out, the rest moved down
        word shift() noexcept {
            word w = a0;
            a0 = a1;
            a1 = a2;
            a2 = 0;
            return w;
        }
    };

    // r = a b / R mod m, a and b below m: Montgomery's product in product
    // scanning, a column of the product at a time, the reduction's quotient
    // word of the column found as soon as its low word is known (the finely
    // integrated form of Koç, Acar and Kaliski, 1996). The products of a
    // and b and those of the quotients and m go into two accumulators, two
    // chains of carries the processor runs side by side (measured on an
    // Apple M2: a sixth faster than one chain, a third faster than the
    // operand-scanning CIOS at 16 words). r may be a or b. t:
    // mul_scratch(k) words
    inline void mont_mul(word* r, const word* a, const word* b, const Modulus& mod, word* t) noexcept {
        const size_t k = mod.k;
        const word* m = mod.m;
        word* q = t;          // k: the quotient words
        word* w = q + k;      // k + 1: the result before its last reduction
        word* u = w + k + 1;  // k
        Accumulator ab;
        Accumulator qm;
        for (size_t i = 0; i < k; ++i) {
            for (size_t j = 0; j < i; ++j) {
                ab.add(a[j], b[i - j]);
                qm.add(q[j], m[i - j]);
            }
            ab.take(qm, 0);
            ab.add(a[i], b[0]);
            q[i] = ab.a0 * mod.m0inv;
            ab.add(q[i], m[0]);
            ab.shift();
        }
        for (size_t i = k; i < 2 * k - 1; ++i) {
            for (size_t j = i - k + 1; j < k; ++j) {
                ab.add(a[j], b[i - j]);
                qm.add(q[j], m[i - j]);
            }
            ab.take(qm, 0);
            w[i - k] = ab.shift();
        }
        w[k - 1] = ab.shift();
        w[k] = ab.a0;
        reduce_once(r, w, w[k], m, k, u);
    }

    // r = t / R mod m for t of 2k words below m R (Montgomery's reduction,
    // a word at a time); t is used up. r: k words, not in t
    inline void redc(word* r, word* t, const Modulus& mod) noexcept {
        const size_t k = mod.k;
        const word* m = mod.m;
        word top = 0;
        for (size_t i = 0; i < k; ++i) {
            word q = t[i] * mod.m0inv;
            word c = 0;
            for (size_t j = 0; j < k; ++j) {
                wide x = wide(q) * m[j] + t[i + j] + c;
                t[i + j] = word(x);
                c = word(x >> 64);
            }
            wide z = wide(t[i + k]) + c + top;
            t[i + k] = word(z);
            top = word(z >> 64);
        }
        reduce_once(r, t + k, top, m, k, t);
    }

    // r = a^2 / R mod m, as mont_mul but each cross product a[j] a[i - j]
    // of a column made once and doubled, the square of the middle word
    // added: k (k + 1) / 2 products of a against mont_mul's k^2. t:
    // mul_scratch(k) words
    inline void mont_sqr(word* r, const word* a, const Modulus& mod, word* t) noexcept {
        const size_t k = mod.k;
        const word* m = mod.m;
        word* q = t;
        word* w = q + k;
        word* u = w + k + 1;
        Accumulator acc;
        Accumulator cross;
        Accumulator qm;
        for (size_t i = 0; i < 2 * k - 1; ++i) {
            size_t lo = i < k ? 0 : i - k + 1;
            size_t hi = i < k ? i : k;
            // two chains the processor runs side by side: the quotients
            // times m, and the cross products
            for (size_t j = lo; j < hi; ++j) {
                qm.add(q[j], m[i - j]);
            }
            for (size_t j = lo; 2 * j < i; ++j) {
                cross.add(a[j], a[i - j]);
            }
            acc.take(cross, 1);
            acc.take(qm, 0);
            if (i % 2 == 0) {
                acc.add(a[i / 2], a[i / 2]);
            }
            if (i < k) {
                q[i] = acc.a0 * mod.m0inv;
                acc.add(q[i], m[0]);
                acc.shift();
            } else {
                w[i - k] = acc.shift();
            }
        }
        w[k - 1] = acc.shift();
        w[k] = acc.a0;
        reduce_once(r, w, w[k], m, k, u);
    }

    // r = (a - b) mod m for a, b below m
    inline void mod_sub(word* r, const word* a, const word* b, const word* m, size_t k) noexcept {
        word borrow = sub(r, a, b, k);
        word mask = ct_bit_mask(borrow);
        word carry = 0;
        for (size_t i = 0; i < k; ++i) {
            wide t = wide(r[i]) + (m[i] & mask) + carry;
            r[i] = word(t);
            carry = word(t >> 64);
        }
    }

    // x doubled modulo m (x below m); u: k words of scratch
    inline void mod_double(word* x, const word* m, size_t k, word* u) noexcept {
        word carry = add(x, x, x, k);
        reduce_once(x, x, carry, m, k, u);
    }

    // R^2 and R^3 mod m for the modulus m of k words (odd, above 1). R mod
    // m by doubling the top power of two below m up to 2^(64k); then, with
    // 64k = t 2^j (t odd), t more doublings give 2^t R, the Montgomery form
    // of 2^t, and j Montgomery squarings give the form of 2^(t 2^j) = R,
    // which is R^2. Constant time but for the count of the first
    // doublings, which is the bit length of m. scratch: 3k + 2 words
    inline void mont_constants(word* rr, word* rrr, const word* m, size_t k, word m0inv, word* scratch) noexcept {
        size_t bits = bit_length(m, k);
        word* x = rr;
        word* u = scratch;
        zero(x, k);
        x[(bits - 1) / 64] = word(1) << ((bits - 1) % 64);
        for (size_t i = bits - 1; i < 64 * k; ++i) {
            mod_double(x, m, k, u);
        }
        size_t j = 6 + size_t(__builtin_ctzll(k));
        size_t t = (64 * k) >> j;
        for (size_t i = 0; i < t; ++i) {
            mod_double(x, m, k, u);
        }
        Modulus mod{m, nullptr, nullptr, m0inv, k};
        for (size_t i = 0; i < j; ++i) {
            mont_sqr(x, x, mod, scratch);
        }
        mont_mul(rrr, rr, rr, mod, scratch);
    }

    // a (below m) into the Montgomery form a R mod m, and back
    inline void to_mont(word* r, const word* a, const Modulus& mod, word* t) noexcept {
        mont_mul(r, a, mod.rr, mod, t);
    }

    inline void from_mont(word* r, const word* a, const Modulus& mod, word* t) noexcept {
        word* one = t + mul_scratch(mod.k);
        zero(one, mod.k);
        one[0] = 1;
        mont_mul(r, a, one, mod, t);
    }

    // x of kx words (kx <= 2k, x below m R) into the Montgomery form of
    // x mod m: one reduction (x / R) and a product with R^3. How a number
    // of the size of n reaches the primes p and q of CRT. r may be x.
    // t: reduce_scratch(k) words
    inline void reduce_to_mont(word* r, const word* x, size_t kx, const Modulus& mod, word* t) noexcept {
        const size_t k = mod.k;
        copy(t, x, kx);
        zero(t + kx, 2 * k - kx);
        word* low = t + mul_scratch(k);
        redc(low, t, mod);
        mont_mul(r, low, mod.rrr, mod, t);
    }

    // Words of scratch for reduce_to_mont
    inline size_t reduce_scratch(size_t k) noexcept {
        return mul_scratch(k) + k;
    }

    // Words of scratch for mont_pow
    inline size_t pow_scratch(size_t k) noexcept {
        return 18 * k + mul_scratch(k) + k;
    }

    // r = a^e mod m in Montgomery form (a and r too), e of ebits bits in
    // words, a secret: a window of four bits, 16 powers of a in a table,
    // each window's power read by scanning the whole table through masks.
    // The count of squarings and products depends on ebits alone. r may be
    // a. scratch: pow_scratch(k) words
    inline void mont_pow(word* r, const word* a, const word* e, size_t ebits, const Modulus& mod, word* scratch) noexcept {
        const size_t k = mod.k;
        word* table = scratch;
        word* acc = table + 16 * k;
        word* sel = acc + k;
        word* t = sel + k;
        // table[0] = R mod m (1 in the form), table[i] = a^i
        word* one = t + mul_scratch(k);
        zero(one, k);
        one[0] = 1;
        mont_mul(table, one, mod.rr, mod, t);
        copy(table + k, a, k);
        for (size_t i = 2; i < 16; ++i) {
            if (i % 2 == 0) {
                mont_sqr(table + i * k, table + (i / 2) * k, mod, t);
            } else {
                mont_mul(table + i * k, table + (i - 1) * k, a, mod, t);
            }
        }
        size_t windows = (ebits + 3) / 4;
        copy(acc, table, k);
        for (size_t w = windows; w-- > 0;) {
            if (w + 1 != windows) {
                mont_sqr(acc, acc, mod, t);
                mont_sqr(acc, acc, mod, t);
                mont_sqr(acc, acc, mod, t);
                mont_sqr(acc, acc, mod, t);
            }
            word bits = (e[w / 16] >> (4 * (w % 16))) & 15;
            zero(sel, k);
            for (size_t i = 0; i < 16; ++i) {
                word mask = ct_eq_mask(i, bits);
                const word* entry = table + i * k;
                for (size_t j = 0; j < k; ++j) {
                    sel[j] |= entry[j] & mask;
                }
            }
            mont_mul(acc, acc, sel, mod, t);
        }
        copy(r, acc, k);
    }

    // r = a^e mod m in Montgomery form for a public exponent e (e >= 1):
    // square and multiply over e's bits, which may be seen. r may be a.
    // scratch: mul_scratch(k) + k words
    inline void mont_pow_public(word* r, const word* a, uint64_t e, const Modulus& mod, word* scratch) noexcept {
        const size_t k = mod.k;
        word* acc = scratch;
        word* t = acc + k;
        copy(acc, a, k);
        int top = 63 - __builtin_clzll(e);
        for (int i = top - 1; i >= 0; --i) {
            mont_sqr(acc, acc, mod, t);
            if ((e >> i) & 1) {
                mont_mul(acc, acc, a, mod, t);
            }
        }
        copy(r, acc, k);
    }

    // --- the one variable-time inverse --------------------------------------

    // Words of scratch for inverse_vartime
    inline size_t inverse_scratch(size_t k) noexcept {
        return 6 * (k + 1);
    }

    // r = a^-1 mod n for an odd n of k words and a in [1, n); false when a
    // and n share a factor. Its time depends on a and n: it is called only
    // on a product of the blinding factor with another random number (the
    // inverse of that product, times the other number, is the inverse of
    // the factor), so what it shows is a random number no one else knows.
    //
    // The binary extended GCD, 31 of its steps at a time on 64-bit
    // approximations of the two numbers (their top 33 bits and low 31
    // bits), as Pornin's "Optimized Binary GCD for Modular Inversion"
    // (2020) has it: the steps give a matrix of small factors f, g applied
    // to the whole numbers once per round, the coefficients kept modulo n
    // with the division by 2^31 of each round done as a Montgomery
    // reduction of 31 bits (so that a = u y and b = v y mod n hold
    // throughout). About 2 log2(n) / 31 rounds of a few passes over k
    // words, against the bit-at-a-time algorithm's 2 log2(n) passes
    inline bool inverse_vartime(word* r, const word* y, const word* n, size_t k, word* scratch) noexcept {
        constexpr unsigned steps = 31;
        using swide = __int128;
        word* a = scratch;
        word* b = a + k + 1;
        word* u = b + k + 1;
        word* v = u + k + 1;
        word* t1 = v + k + 1;
        word* t2 = t1 + k + 1;
        copy(a, y, k);
        copy(b, n, k);
        a[k] = b[k] = 0;
        zero(u, k + 1);
        zero(v, k + 1);
        u[0] = 1;
        const word ninv = mont_m0inv(n[0]);   // -n^-1 mod 2^64
        auto is_negative = [&](const word* x) {
            return int64_t(x[k]) < 0;
        };
        auto negate = [&](word* x) {
            word carry = 1;
            for (size_t i = 0; i <= k; ++i) {
                wide s = wide(~x[i]) + carry;
                x[i] = word(s);
                carry = word(s >> 64);
            }
        };
        // x = (p f + q g) / 2^31, exact, signed in k + 1 words
        auto combine = [&](word* x, const word* p, int64_t f, const word* q, int64_t g) {
            swide carry = 0;
            for (size_t i = 0; i < k; ++i) {
                swide t = swide(p[i]) * f + swide(q[i]) * g + carry;
                x[i] = word(t);
                carry = t >> 64;
            }
            x[k] = word(carry);
            for (size_t i = 0; i < k; ++i) {
                x[i] = x[i] >> steps | x[i + 1] << (64 - steps);
            }
            x[k] = word(int64_t(x[k]) >> steps);
        };
        // x = (p f + q g) / 2^31 mod n, in [0, n): the multiple of n that
        // makes the sum divisible by 2^31 added first
        auto combine_mod = [&](word* x, const word* p, int64_t f, const word* q, int64_t g) {
            word low = p[0] * word(f) + q[0] * word(g);
            word c = (low * ninv) & ((word(1) << steps) - 1);
            swide carry = 0;
            for (size_t i = 0; i < k; ++i) {
                swide t = swide(p[i]) * f + swide(q[i]) * g + swide(n[i]) * swide(c) + carry;
                x[i] = word(t);
                carry = t >> 64;
            }
            x[k] = word(carry);
            for (size_t i = 0; i < k; ++i) {
                x[i] = x[i] >> steps | x[i + 1] << (64 - steps);
            }
            x[k] = word(int64_t(x[k]) >> steps);
            while (is_negative(x)) {
                word cy = 0;
                for (size_t i = 0; i < k; ++i) {
                    wide s = wide(x[i]) + n[i] + cy;
                    x[i] = word(s);
                    cy = word(s >> 64);
                }
                x[k] += cy;
            }
            for (;;) {
                // x >= n: subtract
                bool ge = x[k] != 0;
                if (!ge) {
                    ge = true;
                    for (size_t i = k; i-- > 0;) {
                        if (x[i] != n[i]) {
                            ge = x[i] > n[i];
                            break;
                        }
                    }
                }
                if (!ge) {
                    break;
                }
                word bw = sub(x, x, n, k);
                x[k] -= bw;
            }
        };
        // 64 bits of x of len bits: the top 33 and the low 31
        auto approx = [&](const word* x, size_t len) {
            size_t pos = len - steps - 2;   // the lowest of the top 33 bits
            size_t w = pos / 64;
            size_t sh = pos % 64;
            word top = x[w] >> sh;
            if (sh != 0 && w + 1 <= k) {
                top |= x[w + 1] << (64 - sh);
            }
            top &= (word(1) << (steps + 2)) - 1;
            return top << steps | (x[0] & ((word(1) << steps) - 1));
        };
        size_t rounds = 2 * (2 * 64 * k / (steps - 1) + 4);
        for (size_t round = 0;; ++round) {
            if (zero_mask(a, k + 1) != 0) {
                break;
            }
            if (round == rounds) {
                return false;
            }
            size_t len = bit_length(a, k + 1);
            size_t lb = bit_length(b, k + 1);
            len = len > lb ? len : lb;
            len = len > 2 * (steps + 1) ? len : 2 * (steps + 1);
            word ab = approx(a, len);
            word bb = approx(b, len);
            int64_t f0 = 1, g0 = 0, f1 = 0, g1 = 1;
            for (unsigned j = 0; j < steps; ++j) {
                if (ab & 1) {
                    if (ab < bb) {
                        std::swap(ab, bb);
                        std::swap(f0, f1);
                        std::swap(g0, g1);
                    }
                    ab = (ab - bb) >> 1;
                    f0 -= f1;
                    g0 -= g1;
                } else {
                    ab >>= 1;
                }
                f1 <<= 1;
                g1 <<= 1;
            }
            combine(t1, a, f0, b, g0);
            combine(t2, a, f1, b, g1);
            if (is_negative(t1)) {
                negate(t1);
                f0 = -f0;
                g0 = -g0;
            }
            if (is_negative(t2)) {
                negate(t2);
                f1 = -f1;
                g1 = -g1;
            }
            copy(a, t1, k + 1);
            copy(b, t2, k + 1);
            combine_mod(t1, u, f0, v, g0);
            combine_mod(t2, u, f1, v, g1);
            copy(u, t1, k + 1);
            copy(v, t2, k + 1);
        }
        if (one_mask(b, k + 1) == 0) {
            return false;
        }
        copy(r, v, k);
        return true;
    }
}
