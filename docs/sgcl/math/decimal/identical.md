[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::identical

```cpp
bool identical(const decimal& other) const noexcept;
```

Whether the two are the same value at the same scale — what Java's `equals` asks, where `==` here asks for the value
alone: `1.0` and `1.00` are equal and not identical. NaN is identical to NaN, each infinity to itself.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the decimal compared |

## Return value

`true` when the values and the scales are the same.

## Complexity

Linear in the unscaled parts at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal a("1.0");
    println("{} {} {}", a.identical(math::decimal("1.00")), a.identical(math::decimal("1.0")),
            a == math::decimal("1.00"));
    println(math::decimal("1.00").trim_scale().identical(math::decimal("1")));
}
```

Output:

```text
false true true
true
```

## See also

- [operator==, operator\<=\>](operator_cmp.md): the values alone
- [trim_scale](trim_scale.md): a value at its fewest places
- [sgcl::math::decimal](README.md)
