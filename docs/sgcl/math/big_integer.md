# sgcl::math::big_integer

```cpp
#include "sgcl/math/big_integer.h"   // or "sgcl/math/math.h"

namespace sgcl::math {
    class big_integer;
    class parse_error;
    namespace literals {
        template<char... Digits> big_integer operator""_big();
    }
}
```

`math::big_integer` is a whole number of any size: what Go's `math/big.Int` is, with the manners of an `int`. Operators and all — `a * 2 + 1` is written as it reads, `a < 10` and `a == 0` compare with the language's own integers, `/` cuts towards zero and `%` takes the sign of the dividend as they do for `int` — and the methods are `const` and hand back a new value: `a.to_string(16)`, `a.div_rem(d)`, `a.abs()`. Nothing overflows.

```cpp
using namespace sgcl;
using namespace math::literals;

math::big_integer f = 1;
for (auto i : range(1, 101)) {
    f *= i;
}
string digits = f.to_string();             // 100!, 158 digits
string hex = f.to_string(16);
string field = txt::format("{:>#60x}", f);

auto n = 0xffff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_big;
auto [q, r] = n.div_rem(1'000'000'007);

auto parsed = math::big_integer::parse(input, 16);
if (!parsed) {
    return parsed.error().message();       // "not a digit in base 16 at byte 3"
}
```

## Rules

- **Sixteen bytes, and a small value allocates nothing.** Every value `int64_t` holds lives inside the `big_integer`, always — so equality and the hash need no normalizing, and arithmetic on two of them is the processor's own with a check for overflow. A larger one lives in a managed object of limbs (words of 64 bits), made the way the characters of a [`string`](../core/string.md) are: of the smallest size class that holds them, each class a pool of its own, past about 62 KB a buffer of a range of pages. The limbs are numbers, never traced, never zeroed.
- **A value, shared by copying a word.** A copy of a `big_integer` is two words and a barrier; the object of limbs is shared, read by any number of threads without a lock, kept as a key of a [`map`](../core/map.md) or in an [`im`](../immutable/README.md) container, and never changed while two values have it. `a += b` means `a = a + b`: whoever else holds the old value still has it whole.
- **Written in place when nothing else has it.** The result of an operation that has not been copied since — `sum` in `sum += x`, `f` in `f *= k` — has its object to itself, and `+=`, `-=` and `*=` by a value within `int64_t` write into it while it has room, as Go's `z.Add(z, x)` does without Go's receiver in the API. A copy (or a negation, which shares the limbs) marks the object shared, with one atomic write the first time, and from then on both values allocate their results as any value does; a move hands the object on and leaves zero behind. Nothing of this shows but in the time: a copy never changes with the value it was taken from.
- **Where it lives.** A `big_integer` holds a `tracked_ptr`, so it goes where one may: on a stack, in a managed object, in a container of the library. In a `std::vector` or a global, through [`rooted`](../core/rooted.md) or [`root_ptr`](../core/root_ptr.md).
- **An error of the program throws.** A division, a remainder or a `mod` by zero, a negative shift, a NaN or an infinity made into a number are `domain_error`; a base outside 2 to 36 is `invalid_argument`; `to_bytes(length)` too short and a shift past 2^46 limbs are `length_error`. Go panics in all of these, and `int` would be undefined.
- **An error of data does not.** `parse` of text that is not a number is an [`expected`](../core/expected.md) whose `parse_error` has `offset()`, the byte where the reading stopped, and `message()`.
- **Not constant-time.** The length of a value, the fast paths and the corrections of the division depend on the value. Public numbers only — a modulus, a serial number, an INTEGER of DER; secrets are `crypto`'s.
- **64-bit targets with `unsigned __int128`**: clang and gcc on arm64 and x86-64.

## Members

### Construction

```cpp
big_integer() noexcept;                                    // 0
template<std::integral T> big_integer(T value);            // implicit, any whole number but bool, __int128 too
explicit big_integer(double value);                        // the whole part, cut towards zero; NaN, ±∞ → domain_error
big_integer(bool) = delete;
explicit big_integer(long double) = delete;                // on x86-64 it would arrive rounded to a double

static expected<big_integer, parse_error> parse(const string& text, int base = 10);
static big_integer from_bytes(const slice<const byte>& big_endian);
```

The constructor from a whole number is implicit, so that `a * 2`, `4 * a` and `a == 0` work with no second set of operators; it allocates nothing for any value `int64_t` holds, and so is `noexcept` for every type but the unsigned ones of 64 bits (`uint64_t`, `size_t`, `unsigned long long`, which on macOS are not all one type) and the 128-bit ones.

`parse` reads an optional `+` or `-` and the digits of the base, letters in either case, and nothing else: no prefix, no space, no separator — `parse("ff", 16)`, never a base guessed from `0x`. Leading zeros are allowed, and `-0` is zero. The error's `offset()` is the byte where reading stopped: 0 for empty text, the end when there was only a sign, otherwise the first byte that is not a digit of the base (for a letter past ASCII, the byte its encoding starts at).

`from_bytes` is the unsigned number of the bytes, most significant first, as Go's `SetBytes`; no bytes are zero.

### Arithmetic

```cpp
friend big_integer operator+(const big_integer&, const big_integer&);
friend big_integer operator-(const big_integer&, const big_integer&);
friend big_integer operator*(const big_integer&, const big_integer&);
friend big_integer operator/(const big_integer&, const big_integer&);   // cut towards zero; by zero → domain_error
friend big_integer operator%(const big_integer&, const big_integer&);   // the sign of the dividend
big_integer operator-() const;
big_integer& operator+=(const big_integer&);                         // and -=, *=, /=, %=, &=, |=, ^=; <<= and >>= take a count
big_integer& operator++();                                       // and --, prefix and postfix

big_integer abs() const;
int sign() const noexcept;                                   // -1, 0, 1
big_integer mod(const big_integer& m) const;                         // in [0, |m|) whatever the signs; m == 0 → domain_error
pair<big_integer, big_integer> div_rem(const big_integer& d) const;     // {a / d, a % d}, one division
```

`/` and `%` are C++'s, so generic code computes the same on an `int` and on a `big_integer`: `-7 / 2 == -3`, `-7 % 2 == -1`. The one other remainder there is reason for is the one modular arithmetic wants, `mod`: `big_integer(-7).mod(2) == 1` (Java's `mod`, Go's `Mod`). `INT64_MIN / -1` is `2^63`, as it should be.

### Number theory

```cpp
big_integer pow(int64_t exponent) const;                     // 0^0 == 1; negative → domain_error
big_integer sqrt() const;                                    // floor; of a negative → domain_error
big_integer gcd(const big_integer& other) const;             // never negative; gcd(0, 0) == 0
big_integer lcm(const big_integer& other) const;             // never negative; 0 when either is 0
big_integer mod_pow(const big_integer& exponent, const big_integer& m) const;   // in [0, m)
optional<big_integer> mod_inverse(const big_integer& m) const;                  // in [0, |m|), or nothing
bool is_probable_prime(int rounds = 20) const;               // Baillie–PSW and more rounds; the same answer every time
static big_integer factorial(int64_t n);                     // negative → domain_error
static big_integer binomial(int64_t n, int64_t k);           // 0 when k > n; negative → domain_error
```

`pow` squares its way up (a power of two is a shift), and refuses before it starts a result past 2^52 bits (`length_error`); a negative exponent would not give a whole number and is `domain_error`. `sqrt` is the whole part of the root, the largest `s` with `s * s <= a`: Newton's method from the root of the top half of the bits, so that one division at full length does the work.

`gcd` is Lehmer's: the steps of Euclid's algorithm that the top bits of the two numbers decide are found on single words, and applied to the whole numbers in one pass. `mod_inverse` takes the same steps with the coefficient carried along; an `a` with a factor in common with `m` has no inverse and gives nothing (`optional`), which is an answer and not an error — `m == 0` is the error. Modulo 1 everything is 0.

`mod_pow` is `a^e mod m` in `[0, m)`, a negative `a` taken modulo `m` first. For an odd `m` it multiplies in Montgomery's form, where a product needs no division, over sliding windows of the exponent's bits; an even `m` costs a multiplication and a division a bit. A negative exponent and a modulus of zero or below are `domain_error` (Go's `Exp` takes a negative exponent as an inverse; here that is `mod_inverse` and then `mod_pow`, said as such).

`is_probable_prime` is Baillie–PSW — trial division by the primes up to 211, the strong test to base 2 and the strong Lucas test with Selfridge's parameters — and then `rounds` rounds of Miller–Rabin with bases drawn from a generator seeded by the number itself, so the same number gets the same answer in every run and on every platform. Below 2^64 the answer is exact (no composite that small passes Baillie–PSW); above, no composite is known to pass it, and each further round lets through at most a quarter of the composites that got so far. `rounds` is Go's argument to `ProbablyPrime`, with the same meaning; below zero it is `domain_error`.

`factorial` multiplies the odd parts of `3 … n` in a balanced tree of products and shifts by the twos at the end, `n - popcount(n)` of them; `binomial` is Python's `math.comb`: `binomial(5, 7) == 0`, and a negative argument is `domain_error`.

```cpp
using namespace sgcl;

auto m127 = math::big_integer(2).pow(127) - 1;
bool prime = m127.is_probable_prime();                          // true
auto root = math::big_integer(10).pow(100).sqrt();              // 10^50
auto inverse = math::big_integer(3).mod_inverse(11);            // 4: 3 · 4 = 12 ≡ 1
auto power = math::big_integer(4).mod_pow(13, 497);             // 445
auto hands = math::big_integer::binomial(52, 5);                // 2598960
```

### Bits

```cpp
friend big_integer operator&(const big_integer&, const big_integer&);
friend big_integer operator|(const big_integer&, const big_integer&);
friend big_integer operator^(const big_integer&, const big_integer&);
big_integer operator~() const;                                   // -a - 1
template<std::integral T> friend big_integer operator<<(const big_integer&, T bits);   // a negative signed count → domain_error
template<std::integral T> friend big_integer operator>>(const big_integer&, T bits);   // rounds down: -1 >> 100 == -1

size_t bit_length() const noexcept;                          // of the magnitude: 0 for 0, 8 for 255 and -255
size_t trailing_zeros() const noexcept;                      // 0 for 0
bool bit(size_t index) const noexcept;                       // in two's complement
```

The bits are those of two's complement stretching without end to the left, as in Go and Python: `-1` is all ones, `~a` is `-a - 1`, `(big_integer(-6) & 0xff) == 250`, and bit 1000 of a negative number is one. `>>` is an arithmetic shift, so it divides rounding down. A count of bits is a whole number of any type up to 64 bits, taken as the value it is: `a << a.bit_length()` needs no cast, a negative `int` is `domain_error`, and a `size_t` is never negative, however large (`a >> size_t(-1)` is 0 or -1, `a << size_t(-1)` too long a number); `bit_length` and `trailing_zeros` ask about the magnitude, as Go's `BitLen` and `TrailingZeroBits` do.

### Comparison

```cpp
friend bool operator==(const big_integer&, const big_integer&) noexcept;
friend std::strong_ordering operator<=>(const big_integer&, const big_integer&) noexcept;
template<std::integral T> friend bool operator==(const big_integer&, T);                    // noexcept for T up to 64 bits
template<std::integral T> friend std::strong_ordering operator<=>(const big_integer&, T);   // the same
```

With a whole number of up to 64 bits on either side no `big_integer` is made for it, `UINT64_MAX` included, so nothing can throw; a 128-bit one is made into a `big_integer` first. The comparison is the mathematical one: `big_integer(-1) < 0u` is true, where `-1 < 0u` is not.

### Conversions

```cpp
string to_string(int base = 10) const;                       // 2 to 36, small letters, a minus in front
vector<byte> to_bytes() const;                          // |a|, most significant first, as short as it goes
vector<byte> to_bytes(size_t length) const;             // padded with zeros; too short → length_error
optional<int64_t> to_int64() const noexcept;
optional<uint64_t> to_uint64() const noexcept;               // nothing for a negative value
double to_double() const noexcept;                           // nearest, a tie to even; past the largest double ±∞
```

`to_bytes` writes the magnitude and no sign; the two's complement form of ASN.1's INTEGER is the business of `encoding`. `to_double` rounds as the conversion of an `int64_t` does: `2^53 + 1` is `2^53`, `2^1024 - 2^970` is infinity and one less than that is the largest double.

### The literal

```cpp
using namespace math::literals;
auto a = 123456789012345678901234567890_big;
auto b = 0xffff'ffff'ffff'ffff'ffff_big;
auto c = 0b1010_big;
auto d = 017_big;                                            // 15: octal, as the language reads 017
```

Decimal, hexadecimal after `0x`, binary after `0b`, octal after a leading `0`, and `'` between digits — the integer literals of C++, whose digits the compiler has checked before this sees them; the value is computed by the compiler too, and what is left for the run is the allocation of a value past `int64_t`. (`0o` is not there: C++ reads `0o17_big` as `0` with the suffix `o17_big`.)

### Text, hashing, streams

```cpp
void format_value(txt::format_sink&, const big_integer&, const txt::format_spec&);   // found by txt::format
std::ostream& operator<<(std::ostream&, const big_integer&);                         // the flags of an int
template<> struct std::hash<big_integer>;
```

[`txt::format`](../txt/format.md) writes a `big_integer` as it writes an `int`: `{}` and `{:d}`, `{:x}` `{:X}` `{:o}` `{:b}` `{:B}`, `#` for the prefix, `+` and a space for the sign, and the width, fill and alignment of any field, the zeros of `{:040}` going after the sign and the prefix. A stream writes it as it writes an `int64_t` — the base, `showbase`, `showpos`, `uppercase`, the width, the fill and `left`, `right` or `internal` — but for a negative number in hexadecimal or octal, which is its magnitude after a minus, a number of no fixed width having no two's complement to print. A specification a whole number does not take — `{:.3}`, `{:f}`, `{:c}` — is an error of the compiler in a literal pattern and `nullopt` from a runtime one.

## What it costs

A value within `int64_t` costs what the processor's own arithmetic does and a branch; anything larger allocates its result, one managed object per operation, from a pool of its size class, but for `+=`, `-=` and `*=` by a small value on a value nobody else has, which write in place (above): a loop of `sum += x` over numbers of a hundred limbs costs what Go's `z.Add(z, x)` does, and `f *= k` less. The loops over limbs are C++ without assembler: the sums eight and sixteen limbs a step with the carry kept in the processor's flags through the step (the compiler's add-with-carry builtins), the products of a number by one limb first and their sums after, in two chains of carries — which puts the multiplication, the division, the conversions to text and `mod_pow` ahead of Go's `math/big` and its assembler at every length measured, `gcd` behind it by a tenth or two.

The number theory runs in the time of its arithmetic: `sqrt` and `pow` of a few multiplications and divisions at full length, `gcd` and `mod_inverse` quadratic (Lehmer's method, as Go's), `mod_pow` of a Montgomery product per bit and a fraction, `is_probable_prime` of about `rounds + 3` exponentiations — a long number from outside is a long computation, as in any library.

The multiplication is the schoolbook one up to a few dozen limbs, Karatsuba's above that and Toom's in three parts above a hundred and more, each with a square of its own that takes fewer products (`a * a`, or two values sharing one object); Go's `math/big` has Karatsuba alone. The division is Knuth's up to a divisor or a quotient of a couple of dozen limbs and Burnikel and Ziegler's recursive one above, which costs a few multiplications. Text in a base that is not a power of two — decimal above all — is written and read by divide and conquer over the powers `10^(19·2^i)`, so a million digits either way take the time of a few long multiplications and never the square of the length: `parse` is safe on text from outside. The thresholds between the algorithms are set by measurement (`bench_math sgcl cross`). `bench_math` measures every case against Go's `math/big` (`benchmarks/go/math`).

## Example

```cpp
#include "sgcl/math/math.h"
#include <iostream>

using namespace sgcl;
using namespace math::literals;

int main() {
    // 100!, and its digits
    auto f = math::big_integer::factorial(100);
    string digits = f.to_string();
    std::cout << digits.size() << " digits, " << f.trailing_zeros() << " twos\n";

    // A toy RSA: two primes, a key, a message there and back
    auto p = 0xd5bbb96d30086ec484eba3d7f9caeb07_big;
    auto q = 0xc9a92c4c5a2b8f6e2b8e7f2a3d9c1e05_big;
    while (!p.is_probable_prime()) {
        p += 2;
    }
    while (!q.is_probable_prime()) {
        q += 2;
    }
    math::big_integer n = p * q;
    math::big_integer e = 65537;
    auto d = e.mod_inverse((p - 1).lcm(q - 1));
    if (!d) {
        return 1;
    }
    auto text = math::big_integer::parse("hello", 36);
    if (!text) {
        std::cout << text.error().message() << '\n';
        return 1;
    }
    math::big_integer sealed = text->mod_pow(e, n);
    math::big_integer opened = sealed.mod_pow(*d, n);
    std::cout << opened.to_string(36) << '\n';

    // Text that is not a number is an answer, not an exception
    auto bad = math::big_integer::parse("12x4");
    std::cout << bad.error().message() << '\n';

    std::cout << txt::format("{:#x} {}", math::big_integer(2).pow(100), math::big_integer(10).pow(40).sqrt()) << '\n';
}
```

Output:

```text
158 digits, 97 twos
hello
not a digit in base 10 at byte 2
0x10000000000000000000000000 100000000000000000000
```

## See also

[`rational`](rational.md) — fractions of two `big_integer`s; [`random`](random.md) — `next_int` below a `big_integer`; [`txt::format`](../txt/format.md); [`string`](../core/string.md), whose model of an immutable object shared by a word `big_integer` follows; the module's [README](README.md), with the table of SGCL against Go.
