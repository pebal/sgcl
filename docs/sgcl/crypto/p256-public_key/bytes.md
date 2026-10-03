[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](../p256-public_key.md)

# sgcl::crypto::p256::public_key::bytes

```cpp
array<byte, size> bytes() const noexcept;
```

The point in SEC 1's uncompressed form, `04 ‖ X ‖ Y`, the coordinates big-endian, Go's `ecdh.PublicKey.Bytes`: the
form TLS sends and [from_bytes](from_bytes.md) reads. `size` is 65 on P-256; `p384::public_key::bytes` gives 97
bytes.

## Parameters

None.

## Return value

The `size` bytes of the point, in an array.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"));
    auto point = key->bytes();
    println("{} bytes", point.size());
    println("X {}", encoding::hex::encode(point.as_slice(1, 32)));
    println("Y {}", encoding::hex::encode(point.as_slice(33, 32)));
}
```

Output:

```text
65 bytes
X 60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6
Y 7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299
```

## See also

- [bytes_compressed](bytes_compressed.md): the compressed form
- [to_pkix_der](to_pkix_der.md): the point in a SubjectPublicKeyInfo
- [sgcl::crypto::p256::public_key](../p256-public_key.md)
