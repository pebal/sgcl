[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_bits

```cpp
optional<bits> as_bits() const noexcept;
```

The bits of a BIT STRING, or of an implicitly tagged primitive read as one: an
[asn1::bits](../asn1-bits/README.md), the bytes (a slice of the element, nothing copied) and how many bits of them.
In an element read as BER the bits past the length in the last byte are as the input had them. `nullopt` for
another type or a content that is not a BIT STRING's.

## Parameters

None.

## Return value

The bits, or `nullopt`.

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
    encoding::asn1 usage = encoding::asn1::bit_string(vector<byte>{byte(0x86)}, 7);
    auto bits = *usage.as_bits();
    println("{} bits: digital signature {}, key encipherment {}", bits.length, bits[0], bits[2]);
}
```

Output:

```text
7 bits: digital signature true, key encipherment false
```

## See also

- [bit_string](bit_string.md): one made
- [asn1::bits](../asn1-bits/README.md)
- [sgcl::encoding::asn1](README.md)
