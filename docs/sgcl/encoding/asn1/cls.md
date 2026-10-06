[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::cls

```cpp
tag_class cls() const noexcept;
```

The [class](../asn1-tag_class.md) of the element's tag: universal for the types X.680 names, context-specific
for `[0]`, application and private for the classes a protocol gives its own types (LDAP's messages are of the
application class). Universal for [asn1()](asn1.md).

## Parameters

None.

## Return value

The class.

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
        println("{} {}", int(e.cls()), e.tag());
    }
}
```

Output:

```text
2 0
0 2
2 1
0 12
```

## See also

- [tag](tag.md): the number of the tag
- [asn1::tag_class](../asn1-tag_class.md)
- [sgcl::encoding::asn1](README.md)
