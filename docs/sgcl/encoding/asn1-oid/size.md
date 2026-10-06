[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::size

```cpp
size_t size() const noexcept;
```

The count of arcs, the first two counted as two though DER writes them as one number; 0 for `oid()`.

## Parameters

None.

## Return value

The count.

## Complexity

Linear in the size of the identifier.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::asn1::oid("1.2.840.113549.1.1.11").size());
    println(encoding::asn1::oid().size());
}
```

Output:

```text
7
0
```

## See also

- [arc](arc.md): one of them
- [sgcl::encoding::asn1::oid](README.md)
