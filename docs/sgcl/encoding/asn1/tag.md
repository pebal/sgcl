[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::tag

```cpp
uint32_t tag() const noexcept;
```

The number of the element's tag, which with its [class](cls.md) names its type: 2 and universal is an
INTEGER, 0 and context-specific is `[0]`. Any number below 2^28 is read; 0 for [asn1()](asn1.md).

## Parameters

None.

## Return value

The number.

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
        println("{} {}", e.tag(), e.constructed() ? "constructed" : "primitive");
    }
}
```

Output:

```text
0 constructed
2 primitive
1 primitive
12 primitive
```

## See also

- [cls](cls.md): the class of the tag
- [is](is.md): a universal type by name
- [sgcl::encoding::asn1](README.md)
