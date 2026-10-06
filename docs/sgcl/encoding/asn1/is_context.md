[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::is_context

```cpp
bool is_context(uint32_t number) const noexcept;
```

Whether the element's tag is `[number]`, context-specific: how a schema tells its OPTIONAL components
apart (X.509's `[0]` version, `[3]` extensions; a GeneralName's `[2]` DNS name).

## Parameters

| Parameter | Description |
|---|---|
| `number` | the tag's number |

## Return value

`true` when the tag is `[number]`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 cert = encoding::asn1::sequence({
        encoding::asn1::explicit_tag(0, encoding::asn1::integer(2)),
        encoding::asn1::integer(4096),
        encoding::asn1::implicit_tag(1, encoding::asn1::bit_string(vector<byte>{byte(0x80)}, 1)),
        encoding::asn1::utf8_string("example.com")});

    for (auto e : cert) {
        if (e.is_context(0)) {
            println("version {}", *e[0].as_int() + 1);
        } else if (e.is_context(1)) {
            println("{} bit(s)", e.as_bits()->length);
        }
    }
}
```

Output:

```text
version 3
1 bit(s)
```

## See also

- [is](is.md): a universal type
- [explicit_tag](explicit_tag.md), [implicit_tag](implicit_tag.md)
- [sgcl::encoding::asn1](README.md)
