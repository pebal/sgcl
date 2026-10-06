[sgcl](../README.md) › [encoding](README.md) › [asn1](asn1/README.md)

# sgcl::encoding::asn1::type

```cpp
#include "sgcl/encoding/asn1.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class asn1 {
    public:
        enum class type : uint32_t {
            boolean = 1,
            integer = 2,
            bit_string = 3,
            octet_string = 4,
            null = 5,
            object_identifier = 6,
            object_descriptor = 7,
            external = 8,
            real = 9,
            enumerated = 10,
            embedded_pdv = 11,
            utf8_string = 12,
            relative_oid = 13,
            sequence = 16,
            set = 17,
            numeric_string = 18,
            printable_string = 19,
            t61_string = 20,
            videotex_string = 21,
            ia5_string = 22,
            utc_time = 23,
            generalized_time = 24,
            graphic_string = 25,
            visible_string = 26,
            general_string = 27,
            universal_string = 28,
            character_string = 29,
            bmp_string = 30
        };
    };
}
```

`sgcl::encoding::asn1::type` is the universal types of X.680 §8.6 by the numbers of their tags, what
[is](asn1/is.md) asks an element and a [tag](asn1/tag.md) of the universal class is: Go's `asn1.TagInteger` and
the rest. The library reads and writes the values of most of them; EXTERNAL, REAL, EMBEDDED PDV, RELATIVE-OID and
CHARACTER STRING are read and dumped as their bytes ([content](asn1/content.md)).

| Value | Description |
|---|---|
| `boolean` | BOOLEAN; 1 |
| `integer` | INTEGER; 2 |
| `bit_string` | BIT STRING; 3 |
| `octet_string` | OCTET STRING; 4 |
| `null` | NULL; 5 |
| `object_identifier` | OBJECT IDENTIFIER; 6 |
| `object_descriptor` | ObjectDescriptor; 7 |
| `external` | EXTERNAL; 8 |
| `real` | REAL; 9 |
| `enumerated` | ENUMERATED; 10 |
| `embedded_pdv` | EMBEDDED PDV; 11 |
| `utf8_string` | UTF8String; 12 |
| `relative_oid` | RELATIVE-OID; 13 |
| `sequence` | SEQUENCE, SEQUENCE OF; 16 |
| `set` | SET, SET OF; 17 |
| `numeric_string` | NumericString; 18 |
| `printable_string` | PrintableString; 19 |
| `t61_string` | T61String (TeletexString); 20 |
| `videotex_string` | VideotexString; 21 |
| `ia5_string` | IA5String; 22 |
| `utc_time` | UTCTime; 23 |
| `generalized_time` | GeneralizedTime; 24 |
| `graphic_string` | GraphicString; 25 |
| `visible_string` | VisibleString; 26 |
| `general_string` | GeneralString; 27 |
| `universal_string` | UniversalString; 28 |
| `character_string` | CHARACTER STRING; 29 |
| `bmp_string` | BMPString; 30 |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::sequence({encoding::asn1::null(), encoding::asn1::utf8_string("x")});
    println(e.is(encoding::asn1::type::sequence));
    println(e[1].is(encoding::asn1::type::utf8_string));
    println(e[1].tag() == uint32_t(encoding::asn1::type::utf8_string));
}
```

Output:

```text
true
true
true
```

## See also

- [is](asn1/is.md), [tag](asn1/tag.md)
- [sgcl::encoding::asn1](asn1/README.md)
