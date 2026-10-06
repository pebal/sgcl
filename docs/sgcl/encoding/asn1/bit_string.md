[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::bit_string

```cpp
static asn1 bit_string(const slice<const byte>& bytes) noexcept;          // (1)
static asn1 bit_string(const slice<const byte>& bytes, size_t length);    // (2)
```

A BIT STRING: a count of unused bits, then the bits, the first one the top bit of the first byte.

1. Every bit of the bytes: a key, a signature.
2. The first `length` bits of the bytes: flags such as X.509's key usage. The bytes past the ones the length needs
   are not written, and the bits past the length in the last byte are written zero, as DER requires.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the bits, eight a byte |
| `length` | how many of them; at most eight a byte |

## Return value

The element.

## Complexity

Linear in the bytes written.

## Exceptions

`invalid_argument` when `length` passes the bits of the bytes (2).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> flags{byte(0xA7)};
    encoding::asn1 usage = encoding::asn1::bit_string(flags, 3);
    println(encoding::hex::encode(usage.bytes()));
    auto bits = *usage.as_bits();
    for (size_t i : range(bits.length)) {
        print("{}", bits[i] ? 1 : 0);
    }
    println();
    println(encoding::hex::encode(encoding::asn1::bit_string(flags).bytes()));
}
```

Output:

```text
030205a0
101
030200a7
```

## See also

- [as_bits](as_bits.md): the bits read
- [asn1::bits](../asn1-bits/README.md)
- [sgcl::encoding::asn1](README.md)
