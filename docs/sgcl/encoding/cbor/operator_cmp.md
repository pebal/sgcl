[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::operator== (sgcl::encoding::cbor)

```cpp
friend bool operator==(const cbor& a, const cbor& b) noexcept;
```

Deep equality; `!=` is made from it by the compiler. Values of different kinds are never equal (1 and 1.0, a text
and the same bytes); floats by their bits, every NaN one value; arrays element by element; maps as sets of members,
in any order; tags by their number and content.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the values.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::cbor(1) == encoding::cbor(1.0));
    println(encoding::cbor::map({{"a", 1}, {"b", 2}}) == encoding::cbor::map({{"b", 2}, {"a", 1}}));
    println(encoding::cbor(0.0) == encoding::cbor(-0.0));
}
```

Output:

```text
false
true
false
```

## See also

- [hash](hash.md)
- [sgcl::encoding::cbor](README.md)
