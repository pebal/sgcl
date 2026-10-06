[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::operator== (sgcl::encoding::asn1)

```cpp
friend bool operator==(const asn1& a, const asn1& b) noexcept;
```

Whether two elements are the same bytes, tag, length and content; `!=` is made from it by the compiler.
DER has one encoding of a value, so two DER elements of the same value are equal; an element read as BER is
compared by its definite form. [asn1()](asn1.md) equals only itself.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the elements |

## Return value

`true` when the bytes are the same.

## Complexity

Linear in the size of the elements.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 a = encoding::asn1::utf8_string("x");
    println(a == encoding::asn1::parse(a.bytes()).value());
    println(a == encoding::asn1::printable_string("x"));
    println(encoding::asn1() == encoding::asn1());
}
```

Output:

```text
true
false
true
```

## See also

- [hash](hash.md)
- [sgcl::encoding::asn1](README.md)
