[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_string

```cpp
optional<string> as_string() const noexcept;
```

The text of a string of any type, as UTF-8: a UTF8String as it is; PrintableString, IA5String,
NumericString, VisibleString, UTCTime and GeneralizedTime (ASCII) as they are; a BMPString from UCS-2 and a
UniversalString from UCS-4; a T61String, VideotexString, GraphicString, GeneralString and ObjectDescriptor as
ISO 8859-1, as OpenSSL reads them. An implicitly tagged primitive is read as UTF-8, and one read as BER and
constructed of OCTET STRINGs gives its pieces joined. `nullopt` for another type and for text its type does not
allow.

## Parameters

None.

## Return value

The text, or `nullopt`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(*encoding::asn1::utf8_string("zażółć").as_string());
    println(*encoding::asn1::bmp_string("Zoë").as_string());
    vector<byte> t61{byte(0x14), byte(2), byte(0x4b), byte(0xf6)};
    println(*encoding::asn1::parse(t61)->as_string());
    println(encoding::asn1::integer(1).as_string().has_value());
}
```

Output:

```text
zażółć
Zoë
Kö
false
```

## See also

- [utf8_string](utf8_string.md) and the other strings made
- [sgcl::encoding::asn1](README.md)
