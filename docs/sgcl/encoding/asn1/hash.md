[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::hash

```cpp
size_t hash() const noexcept;
```

A hash of the element's [bytes](bytes.md): two elements [equal](operator_cmp.md) have the same hash. What
`std::hash<encoding::asn1>` gives, so an element is a key of a [map](../../core/map/README.md) or a
[set](../../core/set/README.md).

## Parameters

None.

## Return value

The hash.

## Complexity

Linear in the size of the element.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<encoding::asn1> seen;
    seen.insert(encoding::asn1::integer(1));
    seen.insert(encoding::asn1::parse(encoding::asn1::integer(1).bytes()).value());
    println(seen.size());
}
```

Output:

```text
1
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::encoding::asn1](README.md)
