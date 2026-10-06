[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::object_identifier

```cpp
static asn1 object_identifier(const oid& id);
```

An OBJECT IDENTIFIER of an [oid](../asn1-oid/README.md). A dotted literal converts to an `oid`, checked at
compile time, so `object_identifier(encoding::asn1::oid("2.5.4.3"))` needs nothing more; a text from outside is
[oid::parse](../asn1-oid/parse.md)d first.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the identifier |

## Return value

The element.

## Complexity

Linear in the size of the identifier.

## Exceptions

`invalid_argument` for an `oid()` of no arcs.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 cn = encoding::asn1::object_identifier(encoding::asn1::oid("2.5.4.3"));
    println(encoding::hex::encode(cn.bytes()));
    println(*cn.as_oid());
}
```

Output:

```text
0603550403
2.5.4.3
```

## See also

- [as_oid](as_oid.md): the identifier read
- [asn1::oid](../asn1-oid/README.md)
- [sgcl::encoding::asn1](README.md)
