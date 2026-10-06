[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::bytes

```cpp
slice<const byte> bytes() const noexcept;
```

The whole element, its tag and its length with its content: its DER, what a signature covers (X.509's
TBSCertificate) and what is written out. A slice of the bytes the element was read from or made into, nothing
copied; of an element read from BER with an indefinite length or a string in pieces, its definite form. Empty for
[asn1()](asn1.md).

## Parameters

None.

## Return value

The element's bytes.

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
    encoding::asn1 tbs = encoding::asn1::sequence({encoding::asn1::integer(1), encoding::asn1::null()});
    encoding::asn1 signed_ = encoding::asn1::sequence({tbs, encoding::asn1::bit_string(vector<byte>(4))});
    auto read = encoding::asn1::parse(signed_.bytes());
    println("{}", encoding::hex::encode((*read)[0].bytes()));
    println("{}", (*read)[0] == tbs);
}
```

Output:

```text
30050201010500
true
```

## See also

- [content](content.md): without the tag and the length
- [parse](parse.md)
- [sgcl::encoding::asn1](README.md)
