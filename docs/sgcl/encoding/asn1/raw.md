[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::raw

```cpp
static asn1 raw(tag_class c, uint32_t number, bool constructed, const slice<const byte>& content);
```

An element of any tag over the content given: a type the other functions do not name (a REAL, a
GeneralString), an element taken whole from another structure. The element is checked as [parse](parse.md) checks
DER, so what `raw` makes always reads back: a constructed content must be elements, a universal type's content
what the type allows.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the [class](../asn1-tag_class.md) of the tag |
| `number` | the tag's number, below 2^28 |
| `constructed` | whether the content is elements |
| `content` | the content octets |

## Return value

The element.

## Complexity

Linear in the size of the content.

## Exceptions

`invalid_argument` for a number of 2^28 or more and for an element DER does not allow, with the reason.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> real{byte(0x80), byte(0x01), byte(0x03)};   // 3 * 2^1, X.690 §8.5
    encoding::asn1 e = encoding::asn1::raw(encoding::asn1::tag_class::universal, 9, false, real);
    print(e.to_string());
    try {
        encoding::asn1::raw(encoding::asn1::tag_class::universal, 1, false, vector<byte>{byte(1)});
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
REAL (3 bytes) 800103
sgcl::encoding::asn1::raw: not DER: offset 0: a BOOLEAN not 00 or FF, which DER requires
```

## See also

- [implicit_tag](implicit_tag.md): another element's content under a tag
- [sgcl::encoding::asn1](README.md)
