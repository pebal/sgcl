[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::operator==, operator\<=\> (sgcl::math::big_integer)

```cpp
friend bool operator==(const big_integer& a, const big_integer& b) noexcept;    // (1)
friend std::strong_ordering operator<=>(const big_integer& a,                   // (2)
                                        const big_integer& b) noexcept;
template<std::integral T>
requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
friend bool operator==(const big_integer& a, T b) noexcept;                     // (3)
template<std::integral T>
requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
friend std::strong_ordering operator<=>(const big_integer& a, T b) noexcept;    // (4)
```

Compare two numbers; the order is the mathematical one. `!=`, `<`, `<=`, `>` and `>=` are made from these by the
compiler, and so is the other order of the operands: `0 < a`, `10 == a`.

1. `true` when `a` and `b` are the same number.
2. The order of `a` and `b`.
3. `true` when `a` is the number `b`, a whole number of any type but `bool`.
4. The order of `a` and `b`.

- (3–4) With a whole number of up to 64 bits, `UINT64_MAX` included, no `big_integer` is made for it, so `a < 10`
  and `a == 0` cost a comparison of words; a 128-bit one is made into a `big_integer` first. The comparison is the
  mathematical one, never the language's conversion: `math::big_integer(-1) < 0u` is true, where `-1 < 0u` of two
  `int`s is false. A `bool` and a floating number do not compare: `a == true` and `a < 1.5` do not compile.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the numbers compared |

## Return value

- (1), (3) Whether the numbers are equal.
- (2), (4) `std::strong_ordering::less`, `equal` or `greater`.

## Complexity

- (1–2) Constant when both are within `int64_t`, when the signs differ and when the lengths differ; otherwise
  linear in the number of limbs.
- (3–4) Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <compare>
#include <cstdint>

using namespace sgcl;

int main() {
    auto big = math::big_integer(1) << 64;
    println("{} {} {}", big > UINT64_MAX, big - 1 == UINT64_MAX, 0 < big);
    println("{}", big == ((unsigned __int128)1 << 64));
    println("{}", math::big_integer(-1) < 0u);

    math::big_integer a = 7;
    math::big_integer b = -7;
    println("{} {}", a == b.abs(), (b <=> a) == std::strong_ordering::less);
}
```

Output:

```text
true true true
true
true
true true
```

## See also

- [sign](sign.md): the comparison with zero
- [operator+](operator_arith.md): the arithmetic
- [sgcl::math::big_integer](README.md)
