[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::operator++, operator--

```cpp
big_integer& operator++() noexcept;      // (1)
big_integer& operator--() noexcept;      // (2)
big_integer operator++(int) noexcept;    // (3)
big_integer operator--(int) noexcept;    // (4)
```

Adds or takes one. Nothing overflows: one past `INT64_MAX` is 2^63, one below `INT64_MIN` is -2^63 - 1.

1. `*this = *this + 1`.
2. `*this = *this - 1`.
3. The same as (1), returning the value before.
4. The same as (2), returning the value before.

## Parameters

None.

## Return value

- (1–2) `*this`.
- (3–4) A copy of the value before the change.

## Complexity

Constant within `int64_t`; otherwise linear in the number of limbs.

## Exceptions

None.

## Notes

A step makes a new value as `a + 1` does, also for a value whose object nobody else has; `a += 1` writes such a
value in place ([operator+=](operator_arith.md#notes)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <cstdint>

using namespace sgcl;

int main() {
    math::big_integer top = INT64_MAX;
    math::big_integer bottom = INT64_MIN;
    ++top;
    --bottom;
    println("{} {}", top, bottom);

    math::big_integer n = 10;
    math::big_integer before = n++;
    println("{} {}", before, n);
}
```

Output:

```text
9223372036854775808 -9223372036854775809
10 11
```

## See also

- [operator+=, operator-=](operator_arith.md): the compound assignments
- [sgcl::math::big_integer](README.md)
