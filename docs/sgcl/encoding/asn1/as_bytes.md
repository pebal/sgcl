[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_bytes

```cpp
optional<slice<const byte>> as_bytes() const noexcept;
```

The bytes of an OCTET STRING, or of an implicitly tagged primitive read as one: a slice of the element,
nothing copied. An element of another class read as BER and constructed of OCTET STRINGs (CMS's
`[0] IMPLICIT OCTET STRING` in pieces) gives its pieces joined, into a buffer of their own. `nullopt` for another
type.

## Parameters

None.

## Return value

The bytes, or `nullopt`.

## Complexity

Constant; linear in the bytes for pieces joined.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 digest = encoding::asn1::octet_string(string("abc"));
    println(encoding::hex::encode(*digest.as_bytes()));
    vector<byte> cms = encoding::hex::decode("a080040261620401630000").value();
    auto e = encoding::asn1::parse(cms, encoding::asn1::ber);
    println("{} {}", e->constructed(), encoding::hex::encode(*e->as_bytes()));
    println(encoding::asn1::integer(1).as_bytes().has_value());
}
```

Output:

```text
616263
true 616263
false
```

## See also

- [octet_string](octet_string.md): one made
- [content](content.md): any element's content
- [sgcl::encoding::asn1](README.md)
