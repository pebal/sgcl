[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::implicit_tag

```cpp
static asn1 implicit_tag(uint32_t number, const asn1& inner, tag_class c = tag_class::context_specific);
```

`[number] IMPLICIT`: `inner`'s content under the tag in place of its own, primitive or constructed as
`inner` is — X.509's alternative names (`[2]` for a DNS name, an IA5String), CMS's `[0] IMPLICIT` certificates.
Read back, `as_*` reads the content as the type asked. Of an [asn1()](asn1.md) it is `asn1()`. A tag of the
universal class makes the content that type's, held to its rules.

## Parameters

| Parameter | Description |
|---|---|
| `number` | the tag's number, below 2^28 |
| `inner` | the element whose content is taken |
| `c` | the [class](../asn1-tag_class.md) of the tag, context-specific unless another is given |

## Return value

The element.

## Complexity

Linear in the bytes of `inner`.

## Exceptions

`invalid_argument` for a number of 2^28 or more, or, in the universal class, for a content the type does not allow.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 dns = encoding::asn1::implicit_tag(2, encoding::asn1::ia5_string("example.com"));
    println(encoding::hex::encode(dns.bytes()));
    println("{} {}", dns.is_context(2), *dns.as_string());
}
```

Output:

```text
820b6578616d706c652e636f6d
true example.com
```

## See also

- [explicit_tag](explicit_tag.md): the element inside a tag
- [raw](raw.md): any tag over a content
- [sgcl::encoding::asn1](README.md)
