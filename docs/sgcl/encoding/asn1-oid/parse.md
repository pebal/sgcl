[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::parse

```cpp
static expected<oid, error> parse(const string& text) noexcept;
```

An identifier of a dotted text from outside — a configuration, a request: two arcs at least, the first 0, 1
or 2, the second below 40 under 0 and 1, each in decimal without a leading zero or a sign, and of any size.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the identifier dotted |

## Return value

The identifier, or the [error](../error/README.md): `syntax` for a text that is not an identifier,
`limit_exceeded` for one of more than 63 bytes of DER.

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
    for (const char* text : {"1.3.6.1.4.1.311", "2.25.329800735698586629295641978511506172918", "1.40", "3"}) {
        auto id = encoding::asn1::oid::parse(text);
        if (id) {
            println("{} arcs", id->size());
        } else {
            println(id.error().message());
        }
    }
}
```

Output:

```text
7 arcs
3 arcs
offset 2: not an OBJECT IDENTIFIER
offset 0: not an OBJECT IDENTIFIER
```

## See also

- [(constructor)](asn1-oid.md): a literal
- [sgcl::encoding::asn1::oid](README.md)
