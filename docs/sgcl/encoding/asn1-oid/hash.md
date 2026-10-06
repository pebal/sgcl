[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::hash

```cpp
size_t hash() const noexcept;
```

A hash of the identifier's bytes, what `std::hash<encoding::asn1::oid>` gives: an `oid` is a key of a
[map](../../core/map/README.md) or a [set](../../core/set/README.md), the names of algorithms a table of them.

## Parameters

None.

## Return value

The hash.

## Complexity

Linear in the size of the identifier.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<encoding::asn1::oid, string> names;
    names.insert_or_assign(encoding::asn1::oid("2.5.4.3"), "CN");
    names.insert_or_assign(encoding::asn1::oid("2.5.4.6"), "C");
    println(names.at(encoding::asn1::oid("2.5.4.6")));
}
```

Output:

```text
C
```

## See also

- [operator==, operator\<=\>](operator_cmp.md)
- [sgcl::encoding::asn1::oid](README.md)
