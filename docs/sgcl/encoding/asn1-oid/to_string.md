[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::to_string

```cpp
string to_string() const noexcept;
```

The identifier dotted, each arc in decimal, of any size; empty for `oid()`. What `println("{}", id)`
writes.

## Parameters

None.

## Return value

The text.

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
    encoding::asn1::oid id({2, 18446744073709551615ull});
    println(id.to_string());
    println("[{:>10}]", encoding::asn1::oid("2.5.4.3"));
}
```

Output:

```text
2.18446744073709551615
[   2.5.4.3]
```

## See also

- [parse](parse.md): the other way
- [sgcl::encoding::asn1::oid](README.md)
