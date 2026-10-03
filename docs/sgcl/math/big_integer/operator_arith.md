[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::operator-, operator~, operator+=, operator-=, operator\*=, operator/=, operator%=, operator&=, operator|=, operator^=, operator\<\<=, operator\>\>=, sgcl::math::operator+, operator-, operator\*, operator/, operator%, operator&, operator|, operator^, operator\<\<, operator\>\> (sgcl::math::big_integer)

```cpp
/*(1)*/ friend big_integer operator+(const big_integer& a, const big_integer& b) noexcept;
/*(2)*/ friend big_integer operator-(const big_integer& a, const big_integer& b) noexcept;
/*(3)*/ friend big_integer operator*(const big_integer& a, const big_integer& b) noexcept;
/*(4)*/ friend big_integer operator/(const big_integer& a, const big_integer& b);
/*(5)*/ friend big_integer operator%(const big_integer& a, const big_integer& b);
/*(6)*/ big_integer operator-() const noexcept;
/*(7)*/ friend big_integer operator&(const big_integer& a, const big_integer& b) noexcept;
/*(8)*/ friend big_integer operator|(const big_integer& a, const big_integer& b) noexcept;
/*(9)*/ friend big_integer operator^(const big_integer& a, const big_integer& b) noexcept;
/*(10)*/ big_integer operator~() const noexcept;
/*(11)*/ template<std::integral T>
         requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
         friend big_integer operator<<(const big_integer& a, T bits);
/*(12)*/ template<std::integral T>
         requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
         friend big_integer operator>>(const big_integer& a, T bits)
             noexcept(std::is_unsigned_v<T>);
/*(13)*/ big_integer& operator+=(const big_integer& b) noexcept;
/*(14)*/ big_integer& operator-=(const big_integer& b) noexcept;
/*(15)*/ big_integer& operator*=(const big_integer& b) noexcept;
/*(16)*/ big_integer& operator/=(const big_integer& b);
/*(17)*/ big_integer& operator%=(const big_integer& b);
/*(18)*/ big_integer& operator&=(const big_integer& b) noexcept;
/*(19)*/ big_integer& operator|=(const big_integer& b) noexcept;
/*(20)*/ big_integer& operator^=(const big_integer& b) noexcept;
/*(21)*/ template<std::integral T>
         requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
         big_integer& operator<<=(T bits);
/*(22)*/ template<std::integral T>
         requires (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
         big_integer& operator>>=(T bits) noexcept(std::is_unsigned_v<T>);
```

The arithmetic of whole numbers, as an `int`'s but for overflow, which does not happen. The binary operators are
hidden friends, found through a `big_integer` operand; a whole number of the language converts to one
([constructor](big_integer.md) (2)), so `a * 2 + 1`, `4 * a` and `a % 10` are written as they read.

1. The sum.
2. The difference.
3. The product.
4. The quotient, cut towards zero as an `int`'s: `-7 / 2` is -3. `INT64_MIN / -1` is 2^63, where for an
   `int64_t` it is undefined.
5. The remainder, with the sign of the dividend as an `int`'s: `-7 % 2` is -1, and `a / b * b + a % b` is `a`.
   The remainder that is never negative is [mod](mod.md).
6. The negation. A value past `int64_t` shares its object of limbs with the result, marked shared.
7. The bitwise AND.
8. The bitwise OR.
9. The bitwise XOR.
10. The complement, `-a - 1`.
11. `a` times 2^`bits`.
12. `a` divided by 2^`bits`, rounded down (towards minus infinity) as an arithmetic shift of an `int` is:
    `-1 >> 100` is -1, `-5 >> 1` is -3.
13. `*this = *this + b`.
14. `*this = *this - b`.
15. `*this = *this * b`.
16. `*this = *this / b`.
17. `*this = *this % b`.
18. `*this = *this & b`.
19. `*this = *this | b`.
20. `*this = *this ^ b`.
21. `*this = *this << bits`.
22. `*this = *this >> bits`.

`/` and `%` are C++'s, so that generic code computes the same on an `int` and on a `big_integer`. Go's `Quo` and
`Rem` are these two; its `Div` and `Mod` are Euclidean, and the remainder of that kind, in `[0, |m|)` whatever the
signs, is [mod](mod.md). [div_rem](div_rem.md) gives the quotient and the remainder of one division, as Go's
`QuoRem`.

- (7–10), (18–20) The bits are those of two's complement stretching without end to the left, as in Go and Python:
  -1 is all ones, `~a` is `-a - 1`, `math::big_integer(-6) & 0xff` is 250, and `&`, `|` and `^` of negative
  numbers answer what they would on an `int` wide enough. [bit](bit.md) reads one bit.
- (11–12), (21–22) The count is a whole number of any type up to 64 bits but `bool`, taken as the value it is:
  `a << a.bit_length()` needs no cast, a negative count of a signed type is an error, and a count of an unsigned
  type is never negative, however large: `a >> size_t(-1)` is 0 or -1, and `a << size_t(-1)` of any `a` but zero
  a number too long (`length_error`). A count of 128 bits or a `big_integer` does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the operands; a whole number of the language converts to a `big_integer` |
| `bits` | the count of bits shifted by |

## Return value

- (1–12) The value computed.
- (13–22) `*this`.

## Complexity

`n` is the number of limbs (64 bits each) of the longer operand. When both operands are within `int64_t`, every
operator is the processor's own arithmetic with a check for overflow: constant.

- (1–2), (7–9), (13–14), (18–20) Linear in `n`.
- (3), (15) Linear in `n` when one operand is a single limb. Otherwise the schoolbook multiplication up to a few
  dozen limbs of the shorter operand, quadratic; Karatsuba's above that, `O(n^1.585)`; and Toom's in three parts
  above a hundred and more, `O(n^1.465)`. A square — `a * a`, or two values sharing one object — takes roads of
  its own at each length, with fewer products.
- (4–5), (16–17) Linear in `n` for a divisor of one limb. Otherwise Knuth's algorithm D, quadratic (the length of
  the divisor times that of the quotient), and, for a divisor and a quotient past a couple of dozen limbs, the
  recursive division of Burnikel and Ziegler, which costs a few multiplications.
- (6) Constant.
- (10) Linear in `n`.
- (11–12), (21–22) Linear in the length of the result.

## Exceptions

- (1–3), (6–10), (13–15), (18–20) None.
- (4–5), (16–17) `domain_error` when `b` is zero.
- (11), (21) `domain_error` when `bits` is negative; `length_error` when the result would be past 2^46 limbs (2^52
  bits, half a petabyte).
- (12), (22) `domain_error` when `bits` is negative; none for an unsigned `T`.

A compound assignment that throws leaves the value as it was. Go panics on a division by zero, and for an `int`
it is undefined.

## Notes

A result is written in place when nothing else has the value: the result of an operation not copied since —
`sum` in `sum += x`, `f` in `f *= k` — has its object of limbs to itself, and `+=`, `-=`, and `*=` by a value
within `int64_t` (13–15) write into it while it has room, as Go's `z.Add(z, x)` does, without Go's receiver in
the API. A carry past the room moves the value to an object of the next size class. A copy, or a negation (6),
which shares the limbs, marks the object shared, with one atomic write the first time, and from then on both
values allocate their results as any value does; a move hands the object on. Nothing of this shows but in the
time: a copy never changes with the value it was taken from, and `a += b` means `a = a + b`. Every other result
past `int64_t` is a new managed object of limbs, one per operation, from a pool of its size class.

The loops over limbs are C++ without assembler: the sums eight and sixteen limbs a step, the carry kept in the
processor's flags through the step (the compiler's add-with-carry builtins); the products of a number by one limb
first and their sums after, in two chains of carries. The thresholds between the algorithms are set by
measurement. Go's `math/big` multiplies by Karatsuba's method alone. A multiplication by number-theoretic
transforms, which numbers of millions of digits would want, is still to come.
The times against Go's `math/big` are in [benchmarks](../benchmarks.md#big_integer).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <cstdint>

using namespace sgcl;

int main() {
    auto a = math::big_integer(1) << 100;
    println("{}", a * 3 + 1);
    println("{} {}", a / 1'000'000'007, a % 1'000'000'007);

    math::big_integer m = -7;
    println("{} {} {}", m / 2, m % 2, math::big_integer(INT64_MIN) / -1);
    println("{} {} {} {}", m & 0xff, ~m, m >> 1, math::big_integer(-1) >> 100);
    println("{} {}", a >> 98, (a << a.bit_length()).bit_length());

    math::big_integer f = 1;
    for (int i : range(1, 31)) {
        f *= i;
    }
    println("{}", f);

    try {
        f /= 0;
    } catch (const domain_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
3802951800684688204490109616129
1267650591354675262013 976371285
-3 -1 9223372036854775808
249 6 -4 -1
4 202
265252859812191058636308480000000
sgcl::math::big_integer: division by zero
```

## See also

- [operator++, operator--](operator_inc.md): one up or down
- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [mod](mod.md): the remainder that is never negative
- [div_rem](div_rem.md): the quotient and the remainder of one division
- [pow](pow.md): a power
- [sgcl::math::big_integer](../big_integer.md)
