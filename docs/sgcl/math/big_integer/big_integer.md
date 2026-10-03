[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::big_integer

```cpp
big_integer() noexcept;                                     // (1)
template<std::integral T>
requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
big_integer(T value) noexcept;                              // (2)
explicit big_integer(double value);                         // (3)
template<std::same_as<bool> B>
big_integer(B) = delete;                                    // (4)
explicit big_integer(long double) = delete;                 // (5)
explicit big_integer(const string& text, int base = 10);    // (6)
big_integer(const big_integer& other) noexcept;             // (7)
big_integer(big_integer&& other) noexcept;                  // (8)
```

Constructs a whole number.

1. Zero. Nothing is allocated.
2. From a whole number of any type but `bool`: `char`, `int`, `int64_t`, `uint64_t`, `size_t`, and the
   compiler's `__int128` and `unsigned __int128`. Implicit, so that `a * 2`, `4 * a` and `a == 0` work as with an
   `int`, with no second set of operators; Go writes `big.NewInt(2)` for each constant. A value `int64_t` holds is
   kept inside the `big_integer` and allocates nothing; an unsigned one above `INT64_MAX` and a 128-bit one past
   `int64_t` allocate an object of limbs.
3. The whole part of `value`, cut towards zero as a cast to `int` does: `big_integer(-2.9)` is -2. Exact however
   large: `big_integer(1e30)` is the double's own value, 1000000000000000019884624838656. A `float` is exact as a
   double and is taken by this one too. Explicit.
4. Deleted: a `bool` is not a number, and would otherwise arrive as a double. A template, so that it takes a
   `bool` alone: a plain `big_integer(bool)` would take a pointer as well, and `big_integer("123")` would reach it
   before (6), since `const char*` to `bool` is a standard conversion and beats the user-defined one to `string`.
5. Deleted: a `long double` would arrive rounded to a double. On x86-64 it holds 64 bits of mantissa, and
   2^64 - 1 would come out as 2^64.
6. The number that a text the program itself writes spells in `base`, `big_integer mask("ffff0000", 16)`:
   [parse](parse.md)'s value, or `bad_expected_access<parse_error>` with `parse`'s message. Explicit; the literal
   `0` still goes to (2). A text from outside the program is parsed, and its error is a value. The literal
   [_big](literals.md) is the other way to write a constant, in decimal or after `0x`, `0b` or `0`, its digits
   checked by the compiler.
7. A copy: two words and a barrier. The object of limbs is shared, not copied, and marked shared, with one atomic
   write the first time; from then on neither value writes into it, and a copy never changes with the value it
   was taken from. Go copies the words, `new(big.Int).Set(x)`.
8. Takes the object of limbs of `other` over, and leaves `other` zero. A value within `int64_t`, which has no
   object, is copied, and `other` keeps it.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the number converted |
| `text` | the digits, with an optional sign |
| `base` | the base of the digits, 2 to 36 |
| `other` | the value copied or taken over |

## Complexity

- (1–3), (7–8) Constant.
- (6) That of [parse](parse.md).

## Exceptions

- (1–2), (7–8) None.
- (3) `domain_error` when `value` is a NaN or an infinity.
- (6) `invalid_argument` when `base` is outside 2 to 36; `bad_expected_access<parse_error>` when `text` is not a
  number in `base`, its `error()` the [parse_error](../parse_error/README.md) of `parse`.

## Notes

Go's `big.Int` keeps even a small value in a slice of words, allocated; here a value within `int64_t` is the
`big_integer` itself, sixteen bytes and no object.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <cstdint>

using namespace sgcl;

int main() {
    math::big_integer zero;
    math::big_integer small = -42;
    math::big_integer wide = UINT64_MAX;
    math::big_integer cut(-2.9);
    math::big_integer exact(1e30);
    println("{} {} {} {} {}", zero, small, wide, cut, exact);

    math::big_integer m127("170141183460469231731687303715884105727");
    math::big_integer mask("ffff0000", 16);
    println("{} {}", m127.bit_length(), mask);
    try {
        math::big_integer wrong("12x4");
    } catch (const bad_expected_access<math::parse_error>& e) {
        println("{}", e.error().message());
    }

    math::big_integer copy = m127;
    math::big_integer moved = std::move(m127);
    println("{} {}", copy == moved, m127);
}
```

Output:

```text
0 -42 18446744073709551615 -2 1000000000000000019884624838656
127 4294901760
not a digit in base 10 at byte 2
true 0
```

## See also

- [parse](parse.md): a number from a text from outside the program
- [from_bytes](from_bytes.md): a number from its bytes
- [operator=](operator_assign.md): assigns a value
- [operator""_big](literals.md): a constant of any length
- [sgcl::math::big_integer](README.md)
