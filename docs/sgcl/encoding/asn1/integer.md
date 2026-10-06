[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::integer

```cpp
template<class I> static asn1 integer(I value) noexcept;         // (1)
static asn1 integer(const math::big_integer& value) noexcept;    // (2)
```

An INTEGER in its shortest form of two's complement, as X.690 §8.3 writes it in BER and DER alike: 127 is
`02 01 7F`, 128 is `02 02 00 80`, -129 is `02 02 FF 7F`.

1. Of any integral type but `bool`: takes part only for those; an `uint64_t` past `INT64_MAX` takes nine bytes.
2. Of a [big_integer](../../math/big_integer/README.md) of any size: an RSA modulus, a serial number.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the number |

## Return value

The element.

## Complexity

- (1) Constant.
- (2) Linear in the size of the number.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (int64_t v : {0, 127, 128, -128, -129}) {
        println("{} {}", v, encoding::hex::encode(encoding::asn1::integer(v).bytes()));
    }
    println(encoding::hex::encode(encoding::asn1::integer(uint64_t(-1)).bytes()));
    math::big_integer serial("123456789012345678901234567890");
    encoding::asn1 e = encoding::asn1::integer(serial);
    println("{} {}", encoding::hex::encode(e.bytes()), *e.as_big_integer());
}
```

Output:

```text
0 020100
127 02017f
128 02020080
-128 020180
-129 0202ff7f
020900ffffffffffffffff
020d018ee90ff6c373e0ee4e3f0ad2 123456789012345678901234567890
```

## See also

- [as_int](as_int.md), [as_big_integer](as_big_integer.md): the number read
- [enumerated](enumerated.md): an ENUMERATED
- [sgcl::encoding::asn1](README.md)
