//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "div.h"

#include <algorithm>
#include <string>
#include <vector>

// Magnitudes to text and back, in the bases 2 to 36. A base that is a
// power of two is a matter of cutting bits, linear either way. Any other
// is done a limb's worth of digits at a time — the largest power of the
// base a limb holds, 10^19 for decimal — which is quadratic: n divisions
// of a shrinking number by one limb to write it, n multiplications of a
// growing one to read it. Past a threshold (thresholds.to_string and
// .parse, mul.h) both divide and conquer instead, over the powers
// P_i = (10^19)^(2^i): a number below P_(i+1) is written as its quotient
// and remainder by P_i, each in exactly half the digits, and digits are
// read as the value of the first part times P_i plus that of the last
// 19·2^i digits. That is the cost of a division or a multiplication at
// each of log n levels, O(M(n) log n), where the quadratic loop takes a
// million digits in seconds.
namespace sgcl::math::detail {
    inline constexpr char Digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    // The value of a digit of any base up to 36, either case; 36 for
    // anything that is not one
    constexpr unsigned digit_value(char c) noexcept {
        if (c >= '0' && c <= '9') {
            return unsigned(c - '0');
        }
        if (c >= 'a' && c <= 'z') {
            return unsigned(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'Z') {
            return unsigned(c - 'A' + 10);
        }
        return 36;
    }

    // log2 of the base when it is a power of two, 0 otherwise
    constexpr unsigned power_of_two_bits(unsigned base) noexcept {
        return std::has_single_bit(base) ? unsigned(std::countr_zero(base)) : 0;
    }

    // The largest power of the base that a limb holds, and how many
    // digits it is: 10^19 and 19 for decimal
    struct Chunk {
        Limb power;
        unsigned digits;
    };

    constexpr Chunk chunk_of(unsigned base) noexcept {
        Limb p = base;
        unsigned k = 1;
        while (p <= ~Limb(0) / base) {
            p *= base;
            ++k;
        }
        return {p, k};
    }

    // At most how many digits a magnitude of `bits` bits takes in the
    // base: every digit carries at least floor(log2(base)) bits
    constexpr size_t max_digits(size_t bits, unsigned base) noexcept {
        unsigned per = unsigned(std::bit_width(base) - 1);
        return bits / per + 1;
    }

    // The powers P_0 = c.power, P_i = P_(i-1)^2 of a chunk, as many as
    // asked for; working numbers of the conversions, on the ordinary heap
    // like any other working memory
    using Powers = std::vector<std::vector<Limb>>;

    inline void grow_powers(Powers& powers, Chunk c, size_t count) {
        if (powers.empty()) {
            powers.push_back({c.power});
        }
        while (powers.size() < count) {
            const auto& p = powers.back();
            std::vector<Limb> sq(2 * p.size());
            mul(sq.data(), p.data(), p.size(), p.data(), p.size());
            sq.resize(normalized(sq.data(), sq.size()));
            powers.push_back(std::move(sq));
        }
    }

    // The digits of x (a magnitude below base^width, x[0..xn) not
    // needed after) as exactly `width` characters ending at `end`, zeros
    // in front: a chunk of digits at a time from the bottom, each the
    // remainder of a division by c.power
    inline void write_small(char* end, size_t width, const Limb* x, size_t xn, Chunk c, const Divisor& dv, unsigned base) {
        Scratch s(xn);
        Limb* w = s.get();
        std::memcpy(w, x, xn * sizeof(Limb));
        size_t m = normalized(w, xn);
        char* at = end;
        char* first = end - width;
        while (m) {
            Limb rem = div_1(w, w, m, dv);
            m = normalized(w, m);
            for (unsigned d = 0; d < c.digits && at > first; ++d) {
                *--at = Digits[rem % base];
                rem /= base;
            }
        }
        while (at > first) {
            *--at = '0';
        }
    }

    // The digits of x < P_level as exactly c.digits·2^level characters
    // ending at `end`: its quotient and remainder by P_(level-1), each in
    // half of them
    inline void write_digits(char* end, const Limb* x, size_t xn, size_t level, const Powers& powers, Chunk c, const Divisor& dv, unsigned base) {
        xn = normalized(x, xn);
        size_t width = size_t(c.digits) << level;
        if (!level || xn < std::max<size_t>(thresholds.to_string, 2)) {
            write_small(end, width, x, xn, c, dv, base);
            return;
        }
        const auto& p = powers[level - 1];
        size_t pn = p.size();
        size_t half = width / 2;
        if (compare(x, xn, p.data(), pn) < 0) {
            write_digits(end, x, xn, level - 1, powers, c, dv, base);
            std::memset(end - width, '0', half);
            return;
        }
        size_t qn = xn - pn + 1;
        Scratch qs(qn);
        Scratch rs(pn);
        if (pn == 1) {
            rs.get()[0] = div_1(qs.get(), x, xn, Divisor(p[0]));
        } else {
            divide(qs.get(), rs.get(), x, xn, p.data(), pn);
        }
        write_digits(end, rs.get(), pn, level - 1, powers, c, dv, base);
        write_digits(end - half, qs.get(), qn, level - 1, powers, c, dv, base);
    }

    // The digits of a normalized magnitude, most significant first, no
    // sign, appended to `out`; "0" for zero
    inline void append_digits(std::string& out, const Limb* a, size_t n, unsigned base) {
        if (!n) {
            out += '0';
            return;
        }
        size_t start = out.size();
        if (unsigned k = power_of_two_bits(base)) {
            size_t bits = n * 64 - size_t(std::countl_zero(a[n - 1]));
            out.resize(start + max_digits(bits, base));
            char* to = out.data() + start;
            char* at = to;   // the digits least significant first, reversed at the end
            Limb mask = (Limb(1) << k) - 1;
            for (size_t bit = 0; bit < bits; bit += k) {
                size_t i = bit / 64;
                unsigned o = unsigned(bit % 64);
                Limb d = a[i] >> o;
                if (o + k > 64 && i + 1 < n) {
                    d |= a[i + 1] << (64 - o);
                }
                *at++ = Digits[d & mask];
            }
            std::reverse(to, at);
            out.resize(size_t(at - out.data()));
            return;
        }
        Chunk c = chunk_of(base);
        Divisor dv(c.power);
        size_t width;
        char* to;
        if (n < std::max<size_t>(thresholds.to_string, 2)) {
            size_t bits = n * 64 - size_t(std::countl_zero(a[n - 1]));
            width = (max_digits(bits, base) + c.digits - 1) / c.digits * c.digits;
            out.resize(start + width);
            to = out.data() + start;
            write_small(to + width, width, a, n, c, dv, base);
        } else {
            // The level whose power is above the number: P_level has at
            // least 2·|P_(level-1)| - 1 limbs
            Powers powers;
            grow_powers(powers, c, 1);
            while (2 * powers.back().size() < n + 2) {
                grow_powers(powers, c, powers.size() + 1);
            }
            size_t level = powers.size();
            width = size_t(c.digits) << level;
            out.resize(start + width);
            to = out.data() + start;
            write_digits(to + width, a, n, level, powers, c, dv, base);
        }
        size_t zeros = 0;
        while (zeros + 1 < width && to[zeros] == '0') {
            ++zeros;
        }
        out.erase(start, zeros);
    }

    // At most how many limbs `digits` digits of the base take: every
    // digit carries at most ceil(log2(base)) bits
    constexpr size_t max_limbs(size_t digits, unsigned base) noexcept {
        unsigned per = unsigned(std::bit_width(base - 1));
        return digits / 64 * per + (digits % 64 * per + 63) / 64 + 1;
    }

    // The magnitude of `n` digits already checked to be digits of the
    // base, a chunk at a time: r = r·base^chunk + chunk. r has room for
    // max_limbs(n, base); the length, normalized, returned.
    inline size_t read_small(Limb* r, const char* p, size_t n, Chunk c, unsigned base) noexcept {
        size_t len = 0;
        size_t first = n % c.digits ? n % c.digits : c.digits;
        size_t at = 0;
        for (size_t take = first; at < n; at += take, take = c.digits) {
            Limb chunk = 0;
            Limb scale = 1;
            for (size_t j = at; j < at + take; ++j) {
                chunk = chunk * base + digit_value(p[j]);
                scale *= base;
            }
            // r = r * scale + chunk
            Limb carry = mul_1(r, r, len, scale);
            if (carry) {
                r[len++] = carry;
            }
            for (size_t i = 0; chunk && i < len; ++i) {
                Limb s = r[i] + chunk;
                chunk = s < chunk;
                r[i] = s;
            }
            if (chunk) {
                r[len++] = chunk;
            }
        }
        return len;
    }

    // n <= 2·c.digits·2^level digits: the value of all but the last
    // c.digits·2^level of them times P_level, plus the value of those
    inline size_t read_part(Limb* r, const char* p, size_t n, size_t level, const Powers& powers, Chunk c, unsigned base) {
        if (n < std::max<size_t>(thresholds.parse, 2) * c.digits) {
            return read_small(r, p, n, c, base);
        }
        size_t low = size_t(c.digits) << level;
        if (n <= low) {
            return read_part(r, p, n, level - 1, powers, c, base);
        }
        size_t high = n - low;
        Scratch hs(max_limbs(high, base));
        Scratch ls(max_limbs(low, base));
        size_t hn = read_part(hs.get(), p, high, level - 1, powers, c, base);
        size_t ln = read_part(ls.get(), p + high, low, level - 1, powers, c, base);
        if (!hn) {
            std::memcpy(r, ls.get(), ln * sizeof(Limb));
            return ln;
        }
        const auto& power = powers[level];
        size_t tn = hn + power.size();
        Scratch ts(tn);
        Limb* t = ts.get();
        mul(t, hs.get(), hn, power.data(), power.size());
        Limb carry = add_in(t, tn, ls.get(), ln);
        assert(!carry);
        (void)carry;
        tn = normalized(t, tn);
        std::memcpy(r, t, tn * sizeof(Limb));
        return tn;
    }

    // The magnitude of `n` digits already checked to be digits of the
    // base, written into r (room for max_limbs(n, base)); the length,
    // normalized, returned
    inline size_t read_digits(Limb* r, const char* p, size_t n, unsigned base) {
        if (unsigned k = power_of_two_bits(base)) {
            size_t limbs = (n * k + 63) / 64;
            std::memset(r, 0, limbs * sizeof(Limb));
            size_t bit = 0;
            for (size_t j = n; j-- > 0; bit += k) {
                Limb d = digit_value(p[j]);
                size_t i = bit / 64;
                unsigned o = unsigned(bit % 64);
                r[i] |= d << o;
                if (o + k > 64) {
                    r[i + 1] |= d >> (64 - o);
                }
            }
            return normalized(r, limbs);
        }
        Chunk c = chunk_of(base);
        if (n < std::max<size_t>(thresholds.parse, 2) * c.digits) {
            return read_small(r, p, n, c, base);
        }
        size_t level = 0;
        while ((size_t(c.digits) << (level + 1)) < n) {
            ++level;
        }
        Powers powers;
        grow_powers(powers, c, level + 1);
        return read_part(r, p, n, level, powers, c, base);
    }
}
