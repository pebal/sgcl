[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md)

# sgcl::encoding::asn1::bits

```cpp
#include "sgcl/encoding/asn1.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class asn1 {
    public:
        struct bits {
            slice<const byte> bytes;
            size_t length = 0;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::asn1::bits` is a BIT STRING read by [as_bits](../asn1/as_bits.md): its bytes, a slice of the
element (nothing copied), and how many of their bits it holds, the first bit the top bit of the first byte —
Go's `asn1.BitString`. A key or a signature is whole bytes, `length` a multiple of 8; flags (X.509's key usage) are
asked one by one with [operator\[\]](operator_at.md).

## Rules

- The bits past `length` in the last byte are zero in DER; in an element read as BER they are as the input had
  them.
- The slice keeps the element's bytes alive.

## Member objects

| Object | Description |
|---|---|
| `bytes` | the bytes of the bits, `(length + 7) / 8` of them |
| `length` | how many bits |

## Member functions

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | a bit by its place |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> flags{byte(0xA0)};
    encoding::asn1 usage = encoding::asn1::bit_string(flags, 3);
    encoding::asn1::bits b = *usage.as_bits();
    println("{} bits in {} byte(s)", b.length, b.bytes.size());
}
```

Output:

```text
3 bits in 1 byte(s)
```

## See also

- [as_bits](../asn1/as_bits.md), [bit_string](../asn1/bit_string.md)
- [sgcl::encoding](../README.md)
