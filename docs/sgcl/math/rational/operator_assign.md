[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::rational::operator=

```cpp
rational& operator=(const rational&) noexcept = default;    // (1)
rational& operator=(rational&& other) noexcept;             // (2)
```

Replaces the fraction.

1. With a copy of another: the two parts' objects are shared, nothing is copied but four words.
2. A move, which is the copy (1): the parts' objects are shared, and the fraction moved from keeps its value.

A whole number or a `big_integer` converts to a fraction, so `r = 5` and `r = big_integer(5)` assign `5/1`; a
`double` does not convert by itself, and is assigned as `r = rational(0.5)`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the fraction moved from |

The copy (1) takes the fraction copied, unnamed in the declaration.

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::rational r(1, 3);
    math::rational s;
    s = r;
    r = 5;
    println("{} {}", r, s);
    s = math::rational(0.25);
    println(s);
}
```

Output:

```text
5 1/3
1/4
```

## See also

- [(constructor)](rational.md): the conversions a whole number and a double take
- [sgcl::math::rational](README.md)
