[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::constructed

```cpp
bool constructed() const noexcept;
```

Whether the content is elements (a SEQUENCE, a SET, an explicit tag) rather than bytes. A constructed
element has elements [inside](operator_at.md); a primitive one has a value. `false` for [asn1()](asn1.md).

## Parameters

None.

## Return value

`true` for a constructed element.

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

    println("{} {} {}", cert.constructed(), cert[0].constructed(), cert[1].constructed());
}
```

Output:

```text
true true false
```

## See also

- [size](size.md): the elements inside
- [sgcl::encoding::asn1](README.md)
