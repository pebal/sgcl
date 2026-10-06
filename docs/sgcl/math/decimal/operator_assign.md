[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::operator=

```cpp
decimal& operator=(const decimal&) noexcept;    // (1), implicitly declared
decimal& operator=(decimal&&) noexcept;         // (2), implicitly declared
```

Replaces the decimal, its scale with it.

1. With a copy of another: the unscaled part's object is shared.
2. With another moved: its unscaled part's object is handed on, and the decimal moved from is zero at its scale.

A whole number or a `big_integer` converts to a decimal at scale 0, so `d = 5` assigns `5`; a `double` does not
convert by itself, and is assigned as `d = math::decimal(0.5)` or `d = math::decimal::shortest(x)`.

## Parameters

None: the decimal copied or moved is unnamed in the declarations.

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
    math::decimal d("1.50");
    math::decimal e;
    e = d;
    d = 5;
    println("{} {}", d, e);
    e = math::decimal::shortest(0.25);
    println(e);
}
```

Output:

```text
5 1.50
0.25
```

## See also

- [(constructor)](decimal.md): the conversions a whole number and a double take
- [sgcl::math::decimal](README.md)
