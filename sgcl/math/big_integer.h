//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../txt/format.h"
#include "detail/convert.h"
#include "detail/div.h"
#include "detail/mul.h"
#include "detail/nat.h"
#include "detail/number.h"
#include "detail/storage.h"
#include "random.h"

#include <atomic>
#include <bit>
#include <charconv>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstring>
#include <functional>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

// A whole number of any size: what Go's math/big.Int is, with the
// manners of an int. Operators and all, `a * 2 + 1` as written; the
// methods are const and hand back a new value (a.to_string(16),
// a.div_rem(d)); the only changes to a variable are the compound
// assignments and the increments, which put a new value in it.
//
// Sixteen bytes. A value that fits in int64_t is kept in the second word
// and allocates nothing, always — so equality and the hash need no
// normalizing, and arithmetic on two such values is the processor's own
// with a check for overflow. Anything larger lives in a managed object
// of limbs (detail/storage.h): a copy of a big_integer is two words, the
// object is shared, can be read from any number of threads, kept as a key
// of a map or in a model of the UI — the same terms as a string's. The one
// exception is a value nothing else has seen: the result of an operation,
// never copied since, whose += and -=, and *= by a value within int64_t,
// write into its own object while it has room (sum += x, f *= k: Go's
// z.Add(z, x) without its API). A copy marks the object shared and from
// then on both allocate as every value does; a move hands it on and leaves
// zero behind. And the same place to live: a big_integer
// holds a tracked_ptr, so it goes on a stack, in a managed object or a
// container of the library, and in a std::vector or a global only
// through rooted/root_ptr.
//
// An error of the program — a division by zero, a negative shift, a NaN
// made into a number, a base outside 2 to 36 — throws (domain_error,
// invalid_argument, length_error), as Go panics and where int
// would be undefined; an error of data — text that is not a number — is
// an expected. Nothing here is constant-time, and nothing will be: the
// length of a value, the fast paths and the corrections of the division
// all depend on it. Secrets belong to crypto's own types.
namespace sgcl::math {
    class big_integer;
    class rational;

    // Why text did not read as a number: where the reading stopped (the
    // byte of the input: 0 for empty text, the first byte that is not a
    // digit of the base, the end when there was only a sign; for a
    // rational also the zero of a denominator and the start of an exponent
    // past a million) and a sentence saying so
    class parse_error {
    public:
        size_t offset() const noexcept {
            return _offset;
        }

        string message() const {
            std::string m;
            switch (_reason) {
                case Reason::empty:
                    m = "empty text";
                    break;
                case Reason::no_digits:
                    m = "no digits after the sign";
                    break;
                case Reason::invalid_digit:
                    m = "not a digit in base " + std::to_string(_base);
                    break;
                case Reason::zero_denominator:
                    m = "a denominator of zero";
                    break;
                case Reason::exponent_out_of_range:
                    m = "an exponent past a million";
                    break;
            }
            m += " at byte " + std::to_string(_offset);
            return string(m);
        }

        friend bool operator==(const parse_error&, const parse_error&) noexcept = default;

    private:
        enum class Reason : uint8_t {
            empty,
            no_digits,
            invalid_digit,
            zero_denominator,
            exponent_out_of_range
        };

        parse_error(Reason reason, size_t offset, int base) noexcept
        : _offset(offset)
        , _base(base)
        , _reason(reason) {
        }

        size_t _offset;
        int _base;
        Reason _reason;

        friend class big_integer;
        friend class rational;
    };

    namespace detail {
        // The result of an operation while it is being written: a new
        // object of limbs no one else has seen, alive by the state of its
        // slot until it becomes the word of a big_integer (finish) or is
        // dropped for a result that turned out small
        class Result;

        struct BigIntAccess;

        template<char... C>
        struct Literal;
    }

    class big_integer {
        using Limb = detail::Limb;
        using Word = tracked_ptr<const void>;

    public:
        // Zero
        big_integer() noexcept
        : _value(0) {
        }

        // From any whole number but bool, implicitly, so that a * 2,
        // a == 0 and 4 * a work as with an int. Nothing is allocated for a
        // value int64_t holds, which is every one but an unsigned above
        // INT64_MAX and the compiler's 128-bit types
        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        big_integer(T value) noexcept(sizeof(T) < 8 || (sizeof(T) == 8 && std::is_signed_v<T>))
        : _value(0) {
            if constexpr (sizeof(T) < 8 || (sizeof(T) == 8 && std::is_signed_v<T>)) {
                _value = int64_t(value);
            } else if constexpr (sizeof(T) == 8) {
                *this = _of_unsigned(uint64_t(value), false);
            } else {
                static_assert(sizeof(T) == 16, "a whole number of 8, 16, 32, 64 or 128 bits");
                if constexpr (std::is_signed_v<T>) {
                    bool negative = value < 0;
                    unsigned __int128 m = negative ? ~(unsigned __int128)value + 1 : (unsigned __int128)value;
                    *this = _of_wide(m, negative);
                } else {
                    *this = _of_wide(value, false);
                }
            }
        }

        // The whole part of a double, cut towards zero as a cast to int
        // does; NaN and the infinities are domain_error
        explicit big_integer(double value);

        // Not from a bool, which would otherwise arrive as a double, nor
        // from a long double, which would arrive as one rounded: on x86-64
        // it holds 64 bits of mantissa, and 2^64 - 1 would come out as 2^64.
        // A float is exact as a double and is taken.
        big_integer(bool) = delete;
        explicit big_integer(long double) = delete;

        // The number the text writes: an optional sign and the digits of
        // the base (2 to 36; letters in either case), nothing else — no
        // prefix, no space, no separator. A base outside 2 to 36 is
        // invalid_argument.
        static expected<big_integer, parse_error> parse(const string& text, int base = 10);

        // The unsigned number of the bytes, most significant first, as Go's
        // SetBytes; no bytes is zero
        static big_integer from_bytes(const slice<const byte>& big_endian);

        // A copy shares the object and marks it shared; a move hands it on
        // and leaves zero behind, so that one value only ever holds an
        // object it may change (the class comment)
        big_integer(const big_integer& other) noexcept
        : _limbs(other._limbs)
        , _value(other._value) {
            other._share();
        }

        big_integer(big_integer&& other) noexcept
        : _limbs(std::move(other._limbs))
        , _value(other._value) {
            if (_limbs) {
                other._limbs = nullptr;
                other._value = 0;
            }
        }

        big_integer& operator=(const big_integer& other) noexcept {
            if (this != &other) {
                _limbs = other._limbs;
                _value = other._value;
                other._share();
            }
            return *this;
        }

        big_integer& operator=(big_integer&& other) noexcept {
            if (this != &other) {
                _limbs = std::move(other._limbs);
                _value = other._value;
                if (_limbs) {
                    other._limbs = nullptr;
                    other._value = 0;
                }
            }
            return *this;
        }

        // Arithmetic, as int's but for overflow, which does not happen: /
        // cuts towards zero and % takes the sign of the dividend, as in
        // C++; a division by zero is domain_error
        friend big_integer operator+(const big_integer& a, const big_integer& b) {
            if (!a._limbs && !b._limbs) {
                int64_t r;
                if (!__builtin_add_overflow(a._value, b._value, &r)) {
                    return big_integer(r);
                }
                return _of_signed_wide(__int128(a._value) + b._value);
            }
            return _add(a, b, false);
        }

        friend big_integer operator-(const big_integer& a, const big_integer& b) {
            if (!a._limbs && !b._limbs) {
                int64_t r;
                if (!__builtin_sub_overflow(a._value, b._value, &r)) {
                    return big_integer(r);
                }
                return _of_signed_wide(__int128(a._value) - b._value);
            }
            return _add(a, b, true);
        }

        friend big_integer operator*(const big_integer& a, const big_integer& b) {
            if (!a._limbs && !b._limbs) {
                int64_t r;
                if (!__builtin_mul_overflow(a._value, b._value, &r)) {
                    return big_integer(r);
                }
                return _of_signed_wide(__int128(a._value) * b._value);
            }
            return _mul(a, b);
        }

        friend big_integer operator/(const big_integer& a, const big_integer& b) {
            big_integer q;
            _divide(a, b, &q, nullptr);
            return q;
        }

        friend big_integer operator%(const big_integer& a, const big_integer& b) {
            big_integer r;
            _divide(a, b, nullptr, &r);
            return r;
        }

        big_integer operator-() const {
            if (!_limbs) {
                if (_value == INT64_MIN) {
                    return _of_unsigned(uint64_t(1) << 63, false);
                }
                return big_integer(-_value);
            }
            if (_value == 1 && _limb(0) == uint64_t(1) << 63) {
                return big_integer(INT64_MIN);
            }
            _share();
            return big_integer(_limbs, -_value);
        }

        big_integer& operator+=(const big_integer& b) {
            if (!_add_in_place(b, false)) {
                *this = *this + b;
            }
            return *this;
        }

        big_integer& operator-=(const big_integer& b) {
            if (!_add_in_place(b, true)) {
                *this = *this - b;
            }
            return *this;
        }

        big_integer& operator*=(const big_integer& b) {
            if (!_mul_in_place(b)) {
                *this = *this * b;
            }
            return *this;
        }

        big_integer& operator/=(const big_integer& b) {
            return *this = *this / b;
        }

        big_integer& operator%=(const big_integer& b) {
            return *this = *this % b;
        }

        big_integer& operator++() {
            return *this = *this + 1;
        }

        big_integer& operator--() {
            return *this = *this - 1;
        }

        big_integer operator++(int) {
            big_integer old = *this;
            ++*this;
            return old;
        }

        big_integer operator--(int) {
            big_integer old = *this;
            --*this;
            return old;
        }

        // The bits as two's complement stretching without end to the left,
        // as in Go and Python: -1 is all ones, ~a is -a - 1, and & | ^ of
        // negative numbers answer what they would on an int wide enough
        friend big_integer operator&(const big_integer& a, const big_integer& b) {
            if (!a._limbs && !b._limbs) {
                return big_integer(a._value & b._value);
            }
            return _bitwise(a, b, std::bit_and<Limb>());
        }

        friend big_integer operator|(const big_integer& a, const big_integer& b) {
            if (!a._limbs && !b._limbs) {
                return big_integer(a._value | b._value);
            }
            return _bitwise(a, b, std::bit_or<Limb>());
        }

        friend big_integer operator^(const big_integer& a, const big_integer& b) {
            if (!a._limbs && !b._limbs) {
                return big_integer(a._value ^ b._value);
            }
            return _bitwise(a, b, std::bit_xor<Limb>());
        }

        big_integer operator~() const {
            if (!_limbs) {
                return big_integer(~_value);
            }
            return -*this - 1;
        }

        // a · 2^bits; a result past 2^46 limbs is length_error. The
        // count is a whole number of any type up to 64 bits — a size_t
        // from bit_length() as well as an int — taken as its own value: a
        // negative count of a signed type is domain_error, and a
        // count of an unsigned type is never negative, however large
        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
        friend big_integer operator<<(const big_integer& a, T bits) {
            return _shift_left(a, _count(bits));
        }

        // a / 2^bits rounded down (towards minus infinity, as an arithmetic
        // shift of an int does): -1 >> 100 is -1; the count as for <<
        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
        friend big_integer operator>>(const big_integer& a, T bits) {
            return _shift_right(a, _count(bits));
        }

        big_integer& operator&=(const big_integer& b) {
            return *this = *this & b;
        }

        big_integer& operator|=(const big_integer& b) {
            return *this = *this | b;
        }

        big_integer& operator^=(const big_integer& b) {
            return *this = *this ^ b;
        }

        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
        big_integer& operator<<=(T bits) {
            return *this = *this << bits;
        }

        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
        big_integer& operator>>=(T bits) {
            return *this = *this >> bits;
        }

        // The order of the numbers. With a whole number of the language on
        // either side no big_integer is made for it, so a < 10 and a == 0 cost
        // a comparison of words
        friend bool operator==(const big_integer& a, const big_integer& b) noexcept {
            if (!a._limbs || !b._limbs) {
                return !a._limbs && !b._limbs && a._value == b._value;
            }
            if (a._value != b._value) {
                return false;
            }
            size_t n = a._size();
            return a._limbs.get() == b._limbs.get() || !std::memcmp(a._data(), b._data(), n * sizeof(Limb));
        }

        friend std::strong_ordering operator<=>(const big_integer& a, const big_integer& b) noexcept {
            if (!a._limbs && !b._limbs) {
                return a._value <=> b._value;
            }
            int sa = a.sign();
            int sb = b.sign();
            if (sa != sb) {
                return sa <=> sb;
            }
            // One sign; the longer magnitude is the larger
            size_t an;
            size_t bn;
            Limb ar;
            Limb br;
            const Limb* ap = a._magnitude(ar, an);
            const Limb* bp = b._magnitude(br, bn);
            int c = detail::compare(ap, an, bp, bn);
            if (sa < 0) {
                c = -c;
            }
            return c <=> 0;
        }

        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        friend bool operator==(const big_integer& a, T b) noexcept(sizeof(T) <= 8) {
            if constexpr (sizeof(T) <= 8) {
                __int128 v;
                return a._as_wide(v) && v == __int128(b);
            } else {
                return a == big_integer(b);
            }
        }

        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        friend std::strong_ordering operator<=>(const big_integer& a, T b) noexcept(sizeof(T) <= 8) {
            if constexpr (sizeof(T) <= 8) {
                __int128 v;
                if (a._as_wide(v)) {
                    return v <=> __int128(b);
                }
                return a.sign() <=> 0;   // two limbs or more: beyond any 64-bit value
            } else {
                return a <=> big_integer(b);
            }
        }

        // -1, 0 or 1
        int sign() const noexcept {
            if (!_limbs) {
                return (_value > 0) - (_value < 0);
            }
            return _value > 0 ? 1 : -1;
        }

        big_integer abs() const {
            return sign() < 0 ? -*this : *this;
        }

        // The quotient and the remainder of one division: {a / d, a % d}
        pair<big_integer, big_integer> div_rem(const big_integer& d) const {
            big_integer q;
            big_integer r;
            _divide(*this, d, &q, &r);
            return {q, r};
        }

        // The remainder in [0, |m|), whatever the signs — the one modular
        // arithmetic wants (Go's Mod, Java's mod); m == 0 is
        // domain_error
        big_integer mod(const big_integer& m) const {
            big_integer r = *this % m;
            if (r.sign() < 0) {
                r += m.abs();
            }
            return r;
        }

        // The number theory. Each is a const method with a new value for
        // its answer; an argument outside what the question means is
        // domain_error, as a division by zero is.

        // a^exponent by squaring, a power of two by a shift; 0^0 is 1, as
        // in Go and Python. A negative exponent is domain_error (the
        // answer would not be whole), and a result past 2^52 bits
        // length_error before anything is computed.
        big_integer pow(int64_t exponent) const;

        // The whole part of the square root: the largest s with s·s <= a,
        // by Newton's method from the square root of the top half, so that
        // one division at full length does the work. A negative a is
        // domain_error.
        big_integer sqrt() const;

        // The greatest common divisor, never negative: gcd(0, 0) is 0 and
        // gcd(a, 0) is |a|. Lehmer's method: the steps of Euclid's algorithm
        // that the top bits decide, done on single words and applied to the
        // whole numbers at once.
        big_integer gcd(const big_integer& other) const;

        // The least common multiple, never negative; 0 when either is 0
        big_integer lcm(const big_integer& other) const;

        // a^exponent modulo m, in [0, m): Montgomery's multiplication with
        // sliding windows for an odd m, a multiplication and a division
        // per step for an even one; a negative a is taken modulo m first. A
        // negative exponent or an m of zero or below is domain_error.
        big_integer mod_pow(const big_integer& exponent, const big_integer& m) const;

        // The x in [0, |m|) with a·x ≡ 1 modulo m, or nothing when a and m
        // have a common factor; m == 0 is domain_error, and modulo 1 the
        // answer is 0
        optional<big_integer> mod_inverse(const big_integer& m) const;

        // Whether a is prime, as far as Baillie–PSW and `rounds` rounds of
        // Miller–Rabin more can tell: trial division by the primes below
        // 212, the strong test to base 2, the strong Lucas test with
        // Selfridge's parameters, then rounds bases drawn from a generator
        // seeded by a itself — so the answer is the same every time. Below
        // 2^64 it is exact (no composite that small passes Baillie–PSW);
        // above, no composite is known to pass it. A negative number, 0 and
        // 1 are not prime; rounds below zero is domain_error.
        bool is_probable_prime(int rounds = 20) const;

        // n!, by a tree of products over the odd parts of 2 … n and one
        // shift for the twos; a negative n is domain_error
        static big_integer factorial(int64_t n);

        // The number of ways to choose k of n: 0 when k > n, as in Python;
        // a negative n or k is domain_error
        static big_integer binomial(int64_t n, int64_t k);

        // The bits of the magnitude, as Go's BitLen: 0 for zero, 8 for 255
        // and for -255
        size_t bit_length() const noexcept {
            size_t n;
            Limb room;
            const Limb* p = _magnitude(room, n);
            return n ? n * 64 - size_t(std::countl_zero(p[n - 1])) : 0;
        }

        // How many zero bits are below the lowest one (of the magnitude,
        // which for this question is the two's complement too); 0 for zero
        size_t trailing_zeros() const noexcept {
            size_t n;
            Limb room;
            const Limb* p = _magnitude(room, n);
            return n ? detail::trailing_zeros(p, n) : 0;
        }

        // Bit `index` of the two's complement: of a negative number every
        // bit past its length is one
        bool bit(size_t index) const noexcept {
            if (!_limbs) {
                return index >= 63 ? _value < 0 : ((_value >> index) & 1) != 0;
            }
            size_t n = _size();
            const Limb* p = _data();
            auto magnitude_bit = [&](size_t i) {
                return i / 64 < n && ((p[i / 64] >> (i % 64)) & 1) != 0;
            };
            if (_value > 0) {
                return magnitude_bit(index);
            }
            // ~(|a| - 1): below the lowest one of |a| the borrow turned
            // zeros into ones, which the complement turns back; the
            // lowest one itself stays; above it every bit is inverted
            size_t t = detail::trailing_zeros(p, n);
            if (index < t) {
                return false;
            }
            if (index == t) {
                return true;
            }
            return !magnitude_bit(index);
        }

        // The digits in the base (2 to 36, small letters), with a minus in
        // front of a negative number; a base outside that is
        // invalid_argument
        string to_string(int base = 10) const {
            _check_base(base);
            std::string out;
            _append(out, base);
            if (out.size() > string::max_size()) {
                // A string holds 4 G characters; a number of more digits than
                // that (512 MB of limbs, in binary) would be cut short
                throw length_error("sgcl::math::big_integer::to_string: more digits than a string holds");
            }
            return string(out);
        }

        // The magnitude as bytes, most significant first, as short as it
        // goes (no bytes for zero); the sign is not written — two's
        // complement for ASN.1 is the business of encoding
        vector<byte> to_bytes() const {
            return to_bytes((bit_length() + 7) / 8);
        }

        // The same padded with zeros on the left to `length` bytes; a
        // magnitude that takes more is length_error (Go's FillBytes
        // panics there)
        vector<byte> to_bytes(size_t length) const {
            size_t need = (bit_length() + 7) / 8;
            if (need > length) {
                throw length_error("sgcl::math::big_integer::to_bytes: the value takes more bytes than given");
            }
            vector<byte> out(length, byte{0});
            size_t n;
            Limb room;
            const Limb* p = _magnitude(room, n);
            byte* at = out.data() + length;
            for (size_t i = 0; i < need; ++i) {
                *--at = byte(p[i / 8] >> (i % 8 * 8));
            }
            return out;
        }

        // The value, when it fits
        optional<int64_t> to_int64() const noexcept {
            if (!_limbs) {
                return _value;
            }
            return nullopt;
        }

        optional<uint64_t> to_uint64() const noexcept {
            if (!_limbs) {
                if (_value < 0) {
                    return nullopt;
                }
                return uint64_t(_value);
            }
            if (_value == 1) {
                return _limb(0);
            }
            return nullopt;
        }

        // The nearest double, a tie to the even one, as a conversion of an
        // int64_t is; a magnitude past the largest finite double is an
        // infinity of the sign
        double to_double() const noexcept;

    private:
        // A small value, in the second word; the object stays null
        explicit big_integer(int64_t v, std::true_type) noexcept
        : _value(v) {
        }

        big_integer(Word limbs, int64_t signed_size) noexcept
        : _limbs(std::move(limbs))
        , _value(signed_size) {
        }

        // The limbs of a large value, past the object's word of flags, and
        // how many there are
        const Limb* _data() const noexcept {
            return static_cast<const Limb*>(_limbs.get()) + 1;
        }

        // The object's word of flags (detail::LimbShared), written by
        // copies of a value that is itself only read, hence atomic
        std::atomic_ref<Limb> _flags() const noexcept {
            return std::atomic_ref<Limb>(*const_cast<Limb*>(static_cast<const Limb*>(_limbs.get())));
        }

        void _share() const noexcept {
            if (_limbs && !(_flags().load(std::memory_order_relaxed) & detail::LimbShared)) {
                _flags().fetch_or(detail::LimbShared, std::memory_order_relaxed);
            }
        }

        // The limbs to write in place: a large value no other has seen
        Limb* _own_data() noexcept {
            if (!_limbs || (_flags().load(std::memory_order_relaxed) & detail::LimbShared)) {
                return nullptr;
            }
            return const_cast<Limb*>(_data());
        }

        // How many limbs the object has room for: its class's for the
        // present length, never more than it was made with (a class holds
        // every length up to itself, and a value only grows in place
        // within its room)
        size_t _capacity() const noexcept {
            return detail::LimbMaker::capacity(_size() + 1) - 1;
        }

        // The magnitude in place of n limbs with the sign: small when it
        // fits, as Result::finish makes it
        void _settle(size_t n, bool negative) noexcept {
            n = detail::normalized(_data(), n);
            if (n <= 1) {
                Limb m = n ? _limb(0) : 0;
                if (m <= Limb(INT64_MAX)) {
                    _limbs = nullptr;
                    _value = negative ? -int64_t(m) : int64_t(m);
                    return;
                }
                if (negative && m == Limb(1) << 63) {
                    _limbs = nullptr;
                    _value = INT64_MIN;
                    return;
                }
            }
            _value = negative ? -int64_t(n) : int64_t(n);
        }

        bool _add_in_place(const big_integer& b, bool negate_b);
        bool _mul_in_place(const big_integer& b);

        size_t _size() const noexcept {
            return size_t(_value < 0 ? -_value : _value);
        }

        Limb _limb(size_t i) const noexcept {
            return _data()[i];
        }

        // The magnitude as limbs: its own for a large value, `room` holding
        // the one limb of a small one (none for zero)
        const Limb* _magnitude(Limb& room, size_t& n) const noexcept {
            if (_limbs) {
                n = _size();
                return _data();
            }
            room = _value < 0 ? ~uint64_t(_value) + 1 : uint64_t(_value);
            n = room != 0;
            return &room;
        }

        // The value as a 128-bit one when it is within 64 bits of
        // magnitude: every small value and a large one of a single limb
        bool _as_wide(__int128& v) const noexcept {
            if (!_limbs) {
                v = _value;
                return true;
            }
            if (_value == 1) {
                v = __int128(_limb(0));
                return true;
            }
            if (_value == -1) {
                v = -__int128(_limb(0));
                return true;
            }
            return false;
        }

        static void _check_base(int base) {
            if (base < 2 || base > 36) {
                throw invalid_argument("sgcl::math::big_integer: a base outside 2 to 36");
            }
        }

        static big_integer _of_unsigned(uint64_t m, bool negative);
        static big_integer _of_wide(unsigned __int128 m, bool negative);

        static big_integer _of_signed_wide(__int128 v) {
            bool negative = v < 0;
            return _of_wide(negative ? ~(unsigned __int128)v + 1 : (unsigned __int128)v, negative);
        }

        static big_integer _add(const big_integer& a, const big_integer& b, bool negate_b);
        static big_integer _mul(const big_integer& a, const big_integer& b);
        static void _divide(const big_integer& a, const big_integer& b, big_integer* q, big_integer* r);
        template<class Op>
        static big_integer _bitwise(const big_integer& a, const big_integer& b, Op op);
        static big_integer _shift_left(const big_integer& a, uint64_t bits);
        static big_integer _shift_right(const big_integer& a, uint64_t bits);
        static big_integer _isqrt(const big_integer& n);
        static big_integer _range_product(int64_t first, int64_t last, bool odd_parts);

        // A count of bits as an unsigned value; a negative one is an error
        // of the program
        template<class T>
        static uint64_t _count(T bits) {
            if constexpr (std::is_signed_v<T>) {
                if (bits < 0) {
                    throw domain_error("sgcl::math::big_integer: a shift by a negative count");
                }
            }
            return uint64_t(bits);
        }
        void _append(std::string& out, int base) const;

        size_t _hash() const noexcept {
            auto mix = [](uint64_t x) noexcept {
                x ^= x >> 32;
                x *= 0xd6e8feb86659fd93ull;
                x ^= x >> 32;
                return x;
            };
            if (!_limbs) {
                return size_t(mix(uint64_t(_value)));
            }
            uint64_t h = uint64_t(_value) * 0x9e3779b97f4a7c15ull;
            const Limb* p = _data();
            for (size_t i = 0, n = _size(); i < n; ++i) {
                h = mix(h ^ p[i]) + 0x9e3779b97f4a7c15ull;
            }
            return size_t(h);
        }

        Word _limbs;       // null: the value is small, and in _value
        int64_t _value;    // the value, or the number of limbs, negative for a negative value

        friend class detail::Result;
        friend struct detail::BigIntAccess;
        friend struct std::hash<big_integer>;
        template<char...> friend struct detail::Literal;
    };

    static_assert(sizeof(big_integer) == 16);

    namespace detail {
        class Result {
        public:
            // An object for n limbs and its word of flags, not shared
            explicit Result(size_t n)
            : _slot(LimbMaker::make(n + 1))
            , _p(static_cast<Limb*>(_slot.get()) + 1) {
                _p[-1] = 0;
            }

            Limb* data() noexcept {
                return _p;
            }

            // The value of the first n limbs with the sign: small when it
            // fits (the object dropped for the collector), the object's
            // word otherwise
            big_integer finish(size_t n, bool negative) {
                n = normalized(_p, n);
                if (n <= 1) {
                    Limb m = n ? _p[0] : 0;
                    if (m <= Limb(INT64_MAX)) {
                        return big_integer(negative ? -int64_t(m) : int64_t(m), std::true_type{});
                    }
                    if (negative && m == Limb(1) << 63) {
                        return big_integer(INT64_MIN, std::true_type{});
                    }
                }
                return big_integer(tracked_ptr<const void>(std::move(_slot)), negative ? -int64_t(n) : int64_t(n));
            }

        private:
            LimbMaker::Slot _slot;
            Limb* _p;
        };

        // The magnitude of a big_integer as raw limbs, for the library's own
        // code outside the class (random's bounded draw of a big_integer, the
        // number theory of the stages to come)
        struct BigIntAccess {
            static const Limb* magnitude(const big_integer& a, Limb& room, size_t& n) noexcept {
                return a._magnitude(room, n);
            }
        };

        // The number of limbs past which a value is refused as too long
        // rather than attempted: 2^46 limbs is half a petabyte, so no
        // allocation of that size would succeed anyway, and every count
        // of bits below it fits in 64 bits with room to spare
        constexpr size_t MaxLimbs = size_t(1) << 46;
    }

    inline big_integer big_integer::_of_unsigned(uint64_t m, bool negative) {
        if (m <= uint64_t(INT64_MAX)) {
            return big_integer(negative ? -int64_t(m) : int64_t(m), std::true_type{});
        }
        if (negative && m == uint64_t(1) << 63) {
            return big_integer(INT64_MIN, std::true_type{});
        }
        detail::Result r(1);
        r.data()[0] = m;
        return r.finish(1, negative);
    }

    inline big_integer big_integer::_of_wide(unsigned __int128 m, bool negative) {
        if (!(m >> 64)) {
            return _of_unsigned(uint64_t(m), negative);
        }
        detail::Result r(2);
        r.data()[0] = uint64_t(m);
        r.data()[1] = uint64_t(m >> 64);
        return r.finish(2, negative);
    }

    inline big_integer big_integer::_add(const big_integer& a, const big_integer& b, bool negate_b) {
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = a._magnitude(ar, an);
        const Limb* bp = b._magnitude(br, bn);
        bool na = a.sign() < 0;
        bool nb = (b.sign() < 0) != negate_b;
        if (!bn) {
            return a;
        }
        if (!an) {
            return negate_b ? -b : b;
        }
        if (na == nb) {
            if (an < bn) {
                std::swap(ap, bp);
                std::swap(an, bn);
            }
            detail::Result r(an + 1);
            detail::add(r.data(), ap, an, bp, bn);
            return r.finish(an + 1, na);
        }
        int c = detail::compare(ap, an, bp, bn);
        if (!c) {
            return big_integer();
        }
        if (c < 0) {
            std::swap(ap, bp);
            std::swap(an, bn);
            na = nb;
        }
        detail::Result r(an);
        detail::sub(r.data(), ap, an, bp, bn);
        return r.finish(an, na);
    }

    // b added to (negate_b: taken from) this value in its own object, when
    // no other value has the object and it has room for the result; false
    // leaves the value as it was, for the operator to make a new one. b may
    // be this value: the loops read each limb before they write it
    inline bool big_integer::_add_in_place(const big_integer& b, bool negate_b) {
        Limb* ap = _own_data();
        if (!ap) {
            return false;
        }
        size_t bn;
        Limb br;
        const Limb* bp = b._magnitude(br, bn);
        if (!bn) {
            return true;
        }
        size_t an = _size();
        const bool na = _value < 0;
        const bool nb = (b.sign() < 0) != negate_b;
        if (na == nb) {
            if (an >= bn) {
                if (Limb carry = detail::add_in(ap, an, bp, bn)) {
                    if (an == _capacity()) {   // the carry past the room: an object of the next class
                        detail::Result r(an + 1);
                        std::memcpy(r.data(), ap, an * sizeof(Limb));
                        r.data()[an] = carry;
                        *this = r.finish(an + 1, na);
                        return true;
                    }
                    ap[an] = carry;
                    _value = na ? -int64_t(an + 1) : int64_t(an + 1);
                }
                return true;
            }
            if (bn + 1 > _capacity()) {
                return false;
            }
            detail::add(ap, bp, bn, ap, an);
            _settle(bn + 1, na);
            return true;
        }
        if (detail::compare(ap, an, bp, bn) >= 0) {
            detail::sub_in(ap, an, bp, bn);
            _settle(an, na);
            return true;
        }
        if (bn > _capacity()) {
            return false;
        }
        detail::sub(ap, bp, bn, ap, an);
        _settle(bn, nb);
        return true;
    }

    // This value times b, a value within int64_t, in its own object on the
    // terms of _add_in_place
    inline bool big_integer::_mul_in_place(const big_integer& b) {
        if (b._limbs) {
            return false;
        }
        Limb* ap = _own_data();
        if (!ap) {
            return false;
        }
        if (!b._value) {
            *this = big_integer();
            return true;
        }
        const Limb m = b._value < 0 ? ~uint64_t(b._value) + 1 : uint64_t(b._value);
        const bool negative = (_value < 0) != (b._value < 0);
        const size_t an = _size();
        if (Limb carry = detail::mul_1(ap, ap, an, m)) {
            if (an == _capacity()) {
                detail::Result r(an + 1);
                std::memcpy(r.data(), ap, an * sizeof(Limb));
                r.data()[an] = carry;
                *this = r.finish(an + 1, negative);
                return true;
            }
            ap[an] = carry;
            _value = negative ? -int64_t(an + 1) : int64_t(an + 1);
            return true;
        }
        _value = negative ? -int64_t(an) : int64_t(an);
        return true;
    }

    inline big_integer big_integer::_mul(const big_integer& a, const big_integer& b) {
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = a._magnitude(ar, an);
        const Limb* bp = b._magnitude(br, bn);
        if (!an || !bn) {
            return big_integer();
        }
        detail::Result r(an + bn);
        if (bn == 1) {
            r.data()[an] = detail::mul_1(r.data(), ap, an, bp[0]);
        } else if (an == 1) {
            r.data()[bn] = detail::mul_1(r.data(), bp, bn, ap[0]);
        } else {
            detail::mul(r.data(), ap, an, bp, bn);
        }
        return r.finish(an + bn, (a.sign() < 0) != (b.sign() < 0));
    }

    inline void big_integer::_divide(const big_integer& a, const big_integer& b, big_integer* q, big_integer* r) {
        if (b.sign() == 0) {
            throw domain_error("sgcl::math::big_integer: division by zero");
        }
        if (!a._limbs && !b._limbs) {
            if (b._value == -1) {
                // INT64_MIN / -1 does not fit, and is undefined for int64_t
                if (q) {
                    *q = -a;
                }
                if (r) {
                    *r = big_integer();
                }
                return;
            }
            if (q) {
                *q = big_integer(a._value / b._value);
            }
            if (r) {
                *r = big_integer(a._value % b._value);
            }
            return;
        }
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = a._magnitude(ar, an);
        const Limb* bp = b._magnitude(br, bn);
        bool na = a.sign() < 0;
        bool nq = na != (b.sign() < 0);
        if (detail::compare(ap, an, bp, bn) < 0) {
            if (q) {
                *q = big_integer();
            }
            if (r) {
                *r = a;
            }
            return;
        }
        if (bn == 1) {
            detail::Divisor dv(bp[0]);
            if (q) {
                detail::Result qr(an);
                Limb rem = detail::div_1(qr.data(), ap, an, dv);
                *q = qr.finish(an, nq);
                if (r) {
                    *r = _of_unsigned(rem, na);
                }
            } else {
                *r = _of_unsigned(detail::mod_1(ap, an, dv), na);
            }
            return;
        }
        size_t qn = an - bn + 1;
        if (q && r) {
            detail::Result qr(qn);
            detail::Result rr(bn);
            detail::divide(qr.data(), rr.data(), ap, an, bp, bn);
            *q = qr.finish(qn, nq);
            *r = rr.finish(bn, na);
        } else if (q) {
            detail::Result qr(qn);
            detail::divide(qr.data(), nullptr, ap, an, bp, bn);
            *q = qr.finish(qn, nq);
        } else {
            detail::Scratch qs(qn);
            detail::Result rr(bn);
            detail::divide(qs.get(), rr.data(), ap, an, bp, bn);
            *r = rr.finish(bn, na);
        }
    }

    // The two operands as two's complement over one limb more than the
    // longer has (so that the top limb is the sign's alone), combined limb
    // by limb, and the result read back: its top bit is its sign, and a
    // negative one is negated to its magnitude
    template<class Op>
    big_integer big_integer::_bitwise(const big_integer& a, const big_integer& b, Op op) {
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = a._magnitude(ar, an);
        const Limb* bp = b._magnitude(br, bn);
        size_t n = std::max(an, bn) + 1;
        detail::Scratch xs(n);
        detail::Scratch ys(n);
        auto twos = [n](Limb* to, const Limb* p, size_t m, bool negative) {
            std::memcpy(to, p, m * sizeof(Limb));
            std::memset(to + m, 0, (n - m) * sizeof(Limb));
            if (negative) {
                detail::sub_one(to, to, m);
                for (size_t i = 0; i < n; ++i) {
                    to[i] = ~to[i];
                }
            }
        };
        Limb* x = xs.get();
        Limb* y = ys.get();
        twos(x, ap, an, a.sign() < 0);
        twos(y, bp, bn, b.sign() < 0);
        detail::Result r(n);
        Limb* z = r.data();
        for (size_t i = 0; i < n; ++i) {
            z[i] = op(x[i], y[i]);
        }
        bool negative = (z[n - 1] >> 63) != 0;
        if (negative) {
            for (size_t i = 0; i < n; ++i) {
                z[i] = ~z[i];
            }
            Limb carry = 1;
            for (size_t i = 0; i < n && carry; ++i) {
                z[i] += carry;
                carry = z[i] < carry;
            }
        }
        return r.finish(n, negative);
    }

    inline big_integer big_integer::_shift_left(const big_integer& a, uint64_t bits) {
        if (a.sign() == 0 || bits == 0) {
            return a;
        }
        if (!a._limbs && bits < 64) {
            // |a| < 2^63 and a factor below 2^64: the product fits in 128
            // bits, and allocates nothing when it fits in 64
            return _of_signed_wide(__int128(a._value) * (__int128(1) << bits));
        }
        size_t an;
        Limb ar;
        const Limb* ap = a._magnitude(ar, an);
        uint64_t words = bits / 64;
        unsigned s = unsigned(bits % 64);
        if (words >= detail::MaxLimbs || an + words >= detail::MaxLimbs) {
            throw length_error("sgcl::math::big_integer: a shift past the longest number");
        }
        size_t n = an + words + 1;
        detail::Result r(n);
        Limb* z = r.data();
        std::memset(z, 0, words * sizeof(Limb));
        if (s) {
            z[n - 1] = detail::shift_left(z + words, ap, an, s);
        } else {
            std::memcpy(z + words, ap, an * sizeof(Limb));
            z[n - 1] = 0;
        }
        return r.finish(n, a.sign() < 0);
    }

    inline big_integer big_integer::_shift_right(const big_integer& a, uint64_t bits) {
        if (bits == 0) {
            return a;
        }
        if (!a._limbs) {
            return big_integer(bits >= 63 ? (a._value < 0 ? -1 : 0) : a._value >> bits);
        }
        size_t an;
        Limb ar;
        const Limb* ap = a._magnitude(ar, an);
        bool negative = a.sign() < 0;
        uint64_t words = bits / 64;
        unsigned s = unsigned(bits % 64);
        if (words >= an) {
            return big_integer(negative ? -1 : 0);
        }
        // For a negative value the magnitude is rounded up where a bit
        // was shifted out: -(ceil(|a| / 2^bits)) is the floor of a / 2^bits
        bool lost = false;
        if (negative) {
            for (size_t i = 0; i < words && !lost; ++i) {
                lost = ap[i] != 0;
            }
            lost = lost || (s && (ap[words] << (64 - s)) != 0);
        }
        size_t n = an - words;
        detail::Result r(n + 1);
        Limb* z = r.data();
        if (s) {
            detail::shift_right(z, ap + words, n, s);
        } else {
            std::memcpy(z, ap + words, n * sizeof(Limb));
        }
        z[n] = 0;
        if (lost) {
            detail::add_one(z, z, n);
        }
        return r.finish(n + 1, negative);
    }

    inline void big_integer::_append(std::string& out, int base) const {
        if (!_limbs) {
            char buf[72];
            auto res = std::to_chars(buf, buf + sizeof buf, _value, base);
            out.append(buf, res.ptr);
            return;
        }
        if (_value < 0) {
            out += '-';
        }
        detail::append_digits(out, _data(), _size(), unsigned(base));
    }

    inline expected<big_integer, parse_error> big_integer::parse(const string& text, int base) {
        _check_base(base);
        const char* p = text.data();
        size_t size = text.size();
        if (!size) {
            return unexpected<parse_error>(parse_error(parse_error::Reason::empty, 0, base));
        }
        size_t at = 0;
        bool negative = false;
        if (p[0] == '+' || p[0] == '-') {
            negative = p[0] == '-';
            at = 1;
        }
        if (at == size) {
            return unexpected<parse_error>(parse_error(parse_error::Reason::no_digits, at, base));
        }
        for (size_t i = at; i < size; ++i) {
            if (detail::digit_value(p[i]) >= unsigned(base)) {
                return unexpected<parse_error>(parse_error(parse_error::Reason::invalid_digit, i, base));
            }
        }
        while (at + 1 < size && p[at] == '0') {
            ++at;
        }
        size_t n = size - at;
        if (n <= detail::chunk_of(unsigned(base)).digits) {
            uint64_t m = 0;
            for (size_t i = at; i < size; ++i) {
                m = m * unsigned(base) + detail::digit_value(p[i]);
            }
            return _of_unsigned(m, negative);
        }
        size_t room = detail::max_limbs(n, unsigned(base));
        detail::Result r(room);
        size_t len = detail::read_digits(r.data(), p + at, n, unsigned(base));
        return r.finish(len, negative);
    }

    inline big_integer big_integer::from_bytes(const slice<const byte>& big_endian) {
        const byte* p = big_endian.data();
        size_t size = big_endian.size();
        size_t at = 0;
        while (at < size && p[at] == byte{0}) {
            ++at;
        }
        size_t bytes = size - at;
        if (bytes <= 8) {
            uint64_t m = 0;
            for (size_t i = at; i < size; ++i) {
                m = m << 8 | uint8_t(p[i]);
            }
            return _of_unsigned(m, false);
        }
        size_t n = (bytes + 7) / 8;
        detail::Result r(n);
        Limb* z = r.data();
        std::memset(z, 0, n * sizeof(Limb));
        for (size_t i = 0; i < bytes; ++i) {
            z[i / 8] |= Limb(uint8_t(p[size - 1 - i])) << (i % 8 * 8);
        }
        return r.finish(n, false);
    }

    inline big_integer::big_integer(double value)
    : _value(0) {
        if (!std::isfinite(value)) {
            throw domain_error("sgcl::math::big_integer: a NaN or an infinity is no whole number");
        }
        double whole = std::trunc(value);
        if (std::fabs(whole) < 0x1p63) {
            _value = int64_t(whole);
            return;
        }
        // |whole| >= 2^63: its 53 bits of mantissa shifted up by what the
        // exponent says, exactly
        int e;
        double m = std::frexp(std::fabs(whole), &e);   // |whole| = m · 2^e, 0.5 <= m < 1
        uint64_t mantissa = uint64_t(std::ldexp(m, 53));
        *this = big_integer(mantissa) << (e - 53);
        if (value < 0) {
            *this = -*this;
        }
    }

    // The top 64 bits of the magnitude, with every bit below them folded
    // into the lowest as a sticky bit, go through the processor's own
    // conversion, which rounds a 64-bit value to nearest-even; with its
    // top bit set, the lowest bit is 10 places below the last one a
    // double keeps, so the sticky bit breaks exactly the ties that are
    // not ties. The scaling after is exact, or overflows to infinity
    // where the rounded value is past the largest double.
    inline double big_integer::to_double() const noexcept {
        if (!_limbs) {
            return double(_value);
        }
        size_t n = _size();
        const Limb* p = _data();
        double m;
        int scale;
        if (n == 1) {
            m = double(p[0]);
            scale = 0;
        } else {
            unsigned z = unsigned(std::countl_zero(p[n - 1]));
            Limb top = z ? (p[n - 1] << z) | (p[n - 2] >> (64 - z)) : p[n - 1];
            Limb rest = z ? p[n - 2] << z : p[n - 2];
            for (size_t i = 0; i + 2 < n && !rest; ++i) {
                rest |= p[i];
            }
            top |= rest != 0;
            m = double(top);
            size_t bits = n * 64 - z - 64;
            scale = bits > 4096 ? 4096 : int(bits);
        }
        double d = std::ldexp(m, scale);
        return _value < 0 ? -d : d;
    }

    inline big_integer big_integer::pow(int64_t exponent) const {
        if (exponent < 0) {
            throw domain_error("sgcl::math::big_integer::pow: a negative exponent");
        }
        if (exponent == 0) {
            return 1;
        }
        bool negative = sign() < 0 && (exponent & 1);
        size_t bits = bit_length();
        if (bits <= 1) {
            // 0, 1 and -1
            return negative ? big_integer(-1) : abs();
        }
        if (uint64_t(bits - 1) > uint64_t(64 * detail::MaxLimbs) / uint64_t(exponent)) {
            throw length_error("sgcl::math::big_integer::pow: a result past the longest number");
        }
        if (trailing_zeros() == bits - 1) {
            big_integer r = big_integer(1) << (uint64_t(bits - 1) * uint64_t(exponent));
            return negative ? -r : r;
        }
        big_integer base = abs();
        big_integer r = base;
        for (int i = 62 - std::countl_zero(uint64_t(exponent)); i >= 0; --i) {
            r = r * r;
            if ((exponent >> i) & 1) {
                r = r * base;
            }
        }
        return negative ? -r : r;
    }

    // Below 2^126 on the processor's own numbers; above, the root of the
    // top half of the bits, shifted up (within about 2^k of the root), one
    // step of Newton's method (x + n/x)/2 — which never lands below the
    // root, and within a few of it from so near — and the few steps down
    // counted on the remainder n - x², not on new squares
    inline big_integer big_integer::_isqrt(const big_integer& n) {
        size_t bits = n.bit_length();
        if (bits <= 126) {
            size_t count;
            Limb room;
            const Limb* p = n._magnitude(room, count);
            unsigned __int128 v = count ? p[0] : 0;
            if (count > 1) {
                v |= (unsigned __int128)p[1] << 64;
            }
            auto x = (unsigned __int128)std::sqrt(double(v));
            if (x > ~uint64_t(0)) {
                x = ~uint64_t(0);
            }
            while (x * x > v) {
                --x;
            }
            while ((x + 1) * (x + 1) <= v) {
                ++x;
            }
            return big_integer(x);
        }
        size_t k = bits / 4;
        big_integer x = _isqrt(n >> (2 * k)) << k;
        x = (x + n / x) >> 1;
        big_integer rest = n - x * x;
        while (rest.sign() < 0) {
            rest += 2 * x - 1;
            --x;
        }
        return x;
    }

    inline big_integer big_integer::sqrt() const {
        if (sign() < 0) {
            throw domain_error("sgcl::math::big_integer::sqrt: the square root of a negative number");
        }
        return _isqrt(*this);
    }

    inline big_integer big_integer::gcd(const big_integer& other) const {
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = _magnitude(ar, an);
        const Limb* bp = other._magnitude(br, bn);
        if (!an) {
            return other.abs();
        }
        if (!bn) {
            return abs();
        }
        if (an == 1 && bn == 1) {
            return _of_unsigned(detail::gcd_word(ap[0], bp[0]), false);
        }
        detail::Result r(std::min(an, bn));
        size_t n = detail::gcd_lehmer(r.data(), ap, an, bp, bn);
        return r.finish(n, false);
    }

    inline big_integer big_integer::lcm(const big_integer& other) const {
        if (sign() == 0 || other.sign() == 0) {
            return 0;
        }
        return (abs() / gcd(other)) * other.abs();
    }

    inline big_integer big_integer::mod_pow(const big_integer& exponent, const big_integer& m) const {
        if (m.sign() <= 0) {
            throw domain_error("sgcl::math::big_integer::mod_pow: a modulus of zero or below");
        }
        if (exponent.sign() < 0) {
            throw domain_error("sgcl::math::big_integer::mod_pow: a negative exponent");
        }
        if (m == 1) {
            return 0;
        }
        if (exponent == 0) {
            return 1;
        }
        big_integer base = mod(m);
        if (base.sign() == 0 || base == 1) {
            return base;
        }
        if (m.bit(0)) {
            size_t mn;
            size_t bn;
            size_t en;
            Limb mr;
            Limb br;
            Limb er;
            const Limb* mp = m._magnitude(mr, mn);
            const Limb* bp = base._magnitude(br, bn);
            const Limb* ep = exponent._magnitude(er, en);
            detail::Montgomery mg(mp, mn);
            detail::Scratch xs(mn);
            detail::Scratch rs(mn);
            mg.to(xs.get(), bp, bn);
            detail::mod_pow_windows(mg, rs.get(), xs.get(), ep, en);
            detail::Result out(mn);
            mg.from(out.data(), rs.get());
            return out.finish(mn, false);
        }
        // An even modulus: a square, a product and a remainder per bit
        big_integer r = base;
        for (size_t i = exponent.bit_length() - 1; i-- > 0;) {
            r = (r * r) % m;
            if (exponent.bit(i)) {
                r = (r * base) % m;
            }
        }
        return r;
    }

    inline optional<big_integer> big_integer::mod_inverse(const big_integer& m) const {
        if (m.sign() == 0) {
            throw domain_error("sgcl::math::big_integer::mod_inverse: a modulus of zero");
        }
        big_integer modulus = m.abs();
        if (modulus == 1) {
            return big_integer();
        }
        big_integer a = mod(modulus);
        if (a.sign() == 0) {
            return nullopt;
        }
        size_t an;
        size_t mn;
        Limb ar;
        Limb mr;
        const Limb* ap = a._magnitude(ar, an);
        const Limb* mp = modulus._magnitude(mr, mn);
        detail::Result r(mn);
        size_t n = detail::inverse_lehmer(r.data(), ap, an, mp, mn);
        if (!n) {
            return nullopt;
        }
        return r.finish(n, false);
    }

    inline bool big_integer::is_probable_prime(int rounds) const {
        if (rounds < 0) {
            throw domain_error("sgcl::math::big_integer::is_probable_prime: fewer than zero rounds");
        }
        if (sign() <= 0) {
            return false;
        }
        // The primes below 64 as bits of a word, and the odd ones up to 211
        // in products that fit a word, for the trial division
        constexpr uint64_t SmallPrimes = 0x28208a20a08a28acull;
        constexpr uint64_t Products[] = {
            3ull * 5 * 7 * 11 * 13 * 17 * 19 * 23 * 29 * 31 * 37 * 41 * 43 * 47 * 53,
            59ull * 61 * 67 * 71 * 73 * 79 * 83 * 89 * 97 * 101,
            103ull * 107 * 109 * 113 * 127 * 131 * 137 * 139 * 149,
            151ull * 157 * 163 * 167 * 173 * 179 * 181 * 191,
            193ull * 197 * 199 * 211,
        };
        constexpr uint8_t Primes[] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 0,
                                      59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 0,
                                      103, 107, 109, 113, 127, 131, 137, 139, 149, 0,
                                      151, 157, 163, 167, 173, 179, 181, 191, 0,
                                      193, 197, 199, 211, 0};
        size_t n;
        Limb room;
        const Limb* p = _magnitude(room, n);
        if (n == 1 && p[0] < 64) {
            return (SmallPrimes >> p[0]) & 1;
        }
        if (!(p[0] & 1)) {
            return false;
        }
        size_t at = 0;
        for (uint64_t product : Products) {
            Limb r = n == 1 ? p[0] % product : detail::mod_1(p, n, detail::Divisor(product));
            for (; Primes[at]; ++at) {
                if (r % Primes[at] == 0) {
                    return n == 1 && p[0] == Primes[at];
                }
            }
            ++at;
        }
        if (n == 1 && p[0] < 223 * 223) {
            return true;   // no factor up to 211, and 223 is the next prime
        }
        // Lucas's test wants a D with (D/n) = -1, which a square does not
        // have
        big_integer root = sqrt();
        if (root * root == *this) {
            return false;
        }
        detail::Montgomery mg(p, n);
        // n - 1 = d·2^s
        big_integer minus_one = *this - 1;
        size_t s = minus_one.trailing_zeros();
        big_integer d = minus_one >> s;
        size_t dn;
        Limb dr;
        const Limb* dp = d._magnitude(dr, dn);
        detail::Scratch base(n);
        Limb two = 2;
        mg.to(base.get(), &two, 1);
        if (!detail::strong_probable_prime(mg, base.get(), dp, dn, s)) {
            return false;
        }
        if (!detail::strong_lucas_probable_prime(mg, p, n)) {
            return false;
        }
        if (n == 1 || !rounds) {
            return true;
        }
        random draw(p[0] ^ (uint64_t(n) * 0x9e3779b97f4a7c15ull));
        big_integer span = *this - 3;
        for (int i = 0; i < rounds; ++i) {
            big_integer b = draw.next_int(span) + 2;   // [2, n - 2]
            size_t bn;
            Limb br;
            const Limb* bp = b._magnitude(br, bn);
            mg.to(base.get(), bp, bn);
            if (!detail::strong_probable_prime(mg, base.get(), dp, dn, s)) {
                return false;
            }
        }
        return true;
    }

    // The product of first … last (the odd part of each, when asked), as a
    // balanced tree: the leaves gather numbers into a word while it holds
    // them, so the products at the top are of numbers of like length
    inline big_integer big_integer::_range_product(int64_t first, int64_t last, bool odd_parts) {
        if (first > last) {
            return 1;
        }
        if (last - first < 32) {
            big_integer r = 1;
            uint64_t word = 1;
            // Counted from first, so that a last of INT64_MAX ends the loop
            for (int64_t k = 0; k <= last - first; ++k) {
                auto v = uint64_t(first + k);
                if (odd_parts) {
                    v >>= std::countr_zero(v);
                }
                uint64_t product;
                if (__builtin_mul_overflow(word, v, &product)) {
                    r = r * big_integer(word);
                    word = v;
                } else {
                    word = product;
                }
            }
            return r * big_integer(word);
        }
        int64_t middle = first + (last - first) / 2;
        return _range_product(first, middle, odd_parts) * _range_product(middle + 1, last, odd_parts);
    }

    inline big_integer big_integer::factorial(int64_t n) {
        if (n < 0) {
            throw domain_error("sgcl::math::big_integer::factorial: a negative number");
        }
        if (n < 2) {
            return 1;
        }
        // The twos of n! are n minus the ones of n in binary (Legendre)
        auto twos = uint64_t(n) - uint64_t(std::popcount(uint64_t(n)));
        return _range_product(3, n, true) << twos;
    }

    inline big_integer big_integer::binomial(int64_t n, int64_t k) {
        if (n < 0 || k < 0) {
            throw domain_error("sgcl::math::big_integer::binomial: a negative argument");
        }
        if (k > n) {
            return 0;
        }
        k = std::min(k, n - k);
        if (k == 0) {
            return 1;
        }
        return _range_product(n - k + 1, n, false) / _range_product(2, k, false);
    }

    // random's draw of a big_integer, declared with the class in random.h
    inline big_integer random::next_int(const big_integer& bound) {
        if (bound.sign() <= 0) {
            throw domain_error("sgcl::math::random::next_int: a bound of zero or below");
        }
        if (auto small = bound.to_int64()) {
            return next_int(*small);
        }
        size_t bits = bound.bit_length();
        size_t n = (bits + 63) / 64;
        uint64_t mask = bits % 64 ? (uint64_t(1) << (bits % 64)) - 1 : ~uint64_t(0);
        for (;;) {
            detail::Result r(n);
            for (size_t i = 0; i < n; ++i) {
                r.data()[i] = _state.take();
            }
            r.data()[n - 1] &= mask;
            big_integer v = r.finish(n, false);
            if (v < bound) {
                return v;
            }
        }
    }

    namespace literals {
        // A constant of any length, as the language writes an integer:
        // decimal, 0x or 0X hexadecimal, 0b or 0B binary, a leading 0 for
        // octal, and the ' that separates digits anywhere — the digits
        // checked by the compiler, not when the program runs. The value is
        // computed by the compiler too; what is left for the run is the
        // allocation of a value past int64_t.
        //     auto n = 0xffff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_big;
        template<char... C>
        big_integer operator""_big();
    }

    namespace detail {
        template<char... C>
        struct Literal {
            static constexpr char text[] = {C..., '\0'};
            static constexpr size_t length = sizeof...(C);
            static constexpr size_t limbs = length / 16 + 2;   // at most four bits a character

            struct Value {
                Limb limbs[Literal::limbs] = {};
                size_t size = 0;
                bool valid = true;
            };

            static consteval Value compute() {
                Value v;
                size_t at = 0;
                unsigned base = 10;
                if (length > 1 && text[0] == '0') {
                    if (text[1] == 'x' || text[1] == 'X') {
                        base = 16;
                        at = 2;
                    } else if (text[1] == 'b' || text[1] == 'B') {
                        base = 2;
                        at = 2;
                    } else {
                        base = 8;
                        at = 1;
                    }
                }
                bool any = false;
                for (size_t i = at; i < length; ++i) {
                    char c = text[i];
                    if (c == '\'') {
                        continue;
                    }
                    unsigned d = digit_value(c);
                    if (d >= base) {
                        v.valid = false;
                        return v;
                    }
                    any = true;
                    // v = v * base + d
                    Limb carry = d;
                    for (size_t k = 0; k < v.size; ++k) {
                        Wide w = Wide(v.limbs[k]) * base + carry;
                        v.limbs[k] = Limb(w);
                        carry = Limb(w >> 64);
                    }
                    if (carry) {
                        v.limbs[v.size++] = carry;
                    }
                }
                v.valid = any || (length == 1 && text[0] == '0');
                return v;
            }

            static constexpr Value value = compute();

            static big_integer make() {
                static_assert(value.valid, "not a whole number: _big takes the digits of an integer literal");
                if (value.size <= 1) {
                    return big_integer::_of_unsigned(value.size ? value.limbs[0] : 0, false);
                }
                Result r(value.size);
                for (size_t i = 0; i < value.size; ++i) {
                    r.data()[i] = value.limbs[i];
                }
                return r.finish(value.size, false);
            }
        };
    }

    template<char... C>
    big_integer literals::operator""_big() {
        return detail::Literal<C...>::make();
    }

    // txt::format: {} and {:d} decimal, {:x} {:X} {:o} {:b} {:B} in their
    // bases, # for the 0x (0b, a leading 0 in octal), + and a space for
    // the sign of a positive number, and the width, fill and alignment of
    // any number — {:>#60x}, {:040} — the zeros going after the sign and
    // the prefix. The specification is checked where the pattern is
    // compiled (the formatter below says which types it takes): {:.3} and
    // {:f} of a big_integer are errors of the compiler.
    inline void format_value(txt::format_sink& out, const big_integer& v, const txt::format_spec& spec) {
        int base = txt::detail::base_of(spec.type);
        string text = v.abs().to_string(base);
        std::string digits(text.data(), text.size());
        if (spec.type == 'X') {
            for (char& c : digits) {
                if (c >= 'a' && c <= 'z') {
                    c = char(c - 32);
                }
            }
        }
        char head[4];
        size_t head_size = 0;
        if (v.sign() < 0) {
            head[head_size++] = '-';
        } else if (spec.sign == '+' || spec.sign == ' ') {
            head[head_size++] = spec.sign;
        }
        if (spec.alternate && base != 10) {
            if (spec.type == 'o') {
                if (v.sign() != 0) {
                    head[head_size++] = '0';
                }
            } else {
                head[head_size++] = '0';
                head[head_size++] = spec.type;
            }
        }
        txt::detail::put_padded(out, digits, spec, {head, head_size}, '>');
    }

    // As the stream writes an int: decimal, or the base its basefield asks
    // for (std::hex, std::oct); showbase puts 0x (0X with uppercase) or a
    // leading 0 in front of a nonzero value, uppercase writes the digits of
    // hexadecimal in capitals, showpos a plus before a positive decimal;
    // the width is padded with the fill on the side adjustfield says, and
    // std::internal pads between the sign and 0x and the digits (the 0 of
    // octal counts as a digit, as the stream counts it). The
    // width is used up by the one value, as for any number. One thing is
    // not as for an int: a negative number in hexadecimal or octal is its
    // magnitude with a minus, since there is no width of two's complement
    // to write it in.
    inline std::ostream& operator<<(std::ostream& os, const big_integer& v) {
        auto flags = os.flags();
        auto field = flags & std::ios_base::basefield;
        int base = field == std::ios_base::hex ? 16 : field == std::ios_base::oct ? 8 : 10;
        string text = v.abs().to_string(base);
        std::string digits(text.data(), text.size());
        if (base == 16 && (flags & std::ios_base::uppercase)) {
            for (char& c : digits) {
                if (c >= 'a' && c <= 'f') {
                    c = char(c - 32);
                }
            }
        }
        std::string head;
        if (v.sign() < 0) {
            head += '-';
        } else if (base == 10 && (flags & std::ios_base::showpos)) {
            head += '+';
        }
        if ((flags & std::ios_base::showbase) && v.sign() != 0) {
            if (base == 16) {
                head += (flags & std::ios_base::uppercase) ? "0X" : "0x";
            } else if (base == 8) {
                digits.insert(digits.begin(), '0');   // a digit, not a prefix: internal pads before it
            }
        }
        size_t length = head.size() + digits.size();
        size_t width = os.width() > 0 ? size_t(os.width()) : 0;
        std::string out;
        if (width > length) {
            std::string pad(width - length, os.fill());
            auto adjust = flags & std::ios_base::adjustfield;
            if (adjust == std::ios_base::left) {
                out = head + digits + pad;
            } else if (adjust == std::ios_base::internal) {
                out = head + pad + digits;
            } else {
                out = pad + head + digits;
            }
        } else {
            out = head + digits;
        }
        os.width(0);
        return os << out;
    }
}

// Which specifications a big_integer takes, for the pattern checked where it
// is compiled; the writing is format_value's, above
template<>
struct sgcl::txt::formatter<sgcl::math::big_integer> {
    static constexpr bool takes(char type) noexcept {
        return !type || type == 'd' || type == 'b' || type == 'B' || type == 'o'
            || type == 'x' || type == 'X';
    }

    static constexpr bool takes_precision() noexcept {
        return false;
    }
};

template<>
struct std::hash<sgcl::math::big_integer> {
    size_t operator()(const sgcl::math::big_integer& v) const noexcept {
        return v._hash();
    }
};
