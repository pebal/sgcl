[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::hash

```cpp
size_t hash() const noexcept;
```

A hash of the value, equal for [equal](operator_cmp.md) values (a map's members counted in any order): what
`std::hash<encoding::cbor>` gives, so a value is a key of a [map](../../core/map/README.md).

## Parameters

None.

## Return value

The hash.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor a = encoding::cbor::map({{1, 1}, {2, 2}});
    encoding::cbor b = encoding::cbor::map({{2, 2}, {1, 1}});
    println("{} {}", a == b, a.hash() == b.hash());
}
```

Output:

```text
true true
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::encoding::cbor](README.md)
