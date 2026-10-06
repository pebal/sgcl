[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::content

```cpp
slice<const byte> content() const noexcept;
```

The content octets, without the tag and the length: for a primitive element its value as X.690 writes it,
for a constructed one the elements inside. A slice of the element's bytes, nothing copied. Empty for
[asn1()](asn1.md).

## Parameters

None.

## Return value

The content.

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
    encoding::asn1 e = encoding::asn1::integer(65537);
    println("{} {}", encoding::hex::encode(e.bytes()), encoding::hex::encode(e.content()));
}
```

Output:

```text
0203010001 010001
```

## See also

- [bytes](bytes.md): with the tag and the length
- [sgcl::encoding::asn1](README.md)
