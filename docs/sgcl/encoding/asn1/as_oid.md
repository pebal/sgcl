[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_oid

```cpp
optional<oid> as_oid() const noexcept;
```

The [oid](../asn1-oid/README.md) of an OBJECT IDENTIFIER, or of an implicitly tagged primitive read as one.
`nullopt` for another type and for an identifier of more than 63 bytes of DER, which an `oid` does not hold
([content](content.md) still has it).

## Parameters

None.

## Return value

The identifier, or `nullopt`.

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
    encoding::asn1 alg = encoding::asn1::object_identifier(encoding::asn1::oid("1.2.840.10045.4.3.2"));
    auto id = *alg.as_oid();
    println(id);
    println(id == encoding::asn1::oid("1.2.840.10045.4.3.2"));
    println(id.starts_with(encoding::asn1::oid("1.2.840.10045")));
}
```

Output:

```text
1.2.840.10045.4.3.2
true
true
```

## See also

- [object_identifier](object_identifier.md): one made
- [asn1::oid](../asn1-oid/README.md)
- [sgcl::encoding::asn1](README.md)
