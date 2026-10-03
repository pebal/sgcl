[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](../p256-public_key.md)

# sgcl::crypto::p256::public_key::from_bytes

```cpp
static expected<public_key, error> from_bytes(const slice<const byte>& data) noexcept;
```

Reads a point in SEC 1's encoding (§2.3.3): the uncompressed form `04 ‖ X ‖ Y` (65 bytes) or the compressed one,
`02` or `03` (the parity of Y) `‖ X` (33 bytes), whose Y is computed from X. Go's `ecdh.P256().NewPublicKey` takes
the uncompressed form alone. The point is checked: refused are the point at infinity (a single `00`), the
hybrid forms `06` and `07` (as Go refuses them; OpenSSL takes them), any other length or first byte, a coordinate
not below p, a point off the curve and an X with no Y on it. So a `public_key` is always a valid point.

`p384::public_key::from_bytes` takes P-384's points, 97 bytes uncompressed or 49 compressed.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the point, `size` bytes uncompressed or `compressed_size` compressed |

## Return value

The key, or a [crypto::error](../error.md) with `errc::invalid_key` and a message that says which check failed.

## Complexity

Constant: a check that the point is on the curve, and for a compressed point a square root in the field.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the public key of RFC 6979, A.2.5, in both forms
    auto full = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
        "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299"));
    auto compressed = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"));
    println("{}", full == compressed);

    auto infinity = crypto::p256::public_key::from_bytes(vector<byte>(1));
    println("{}", infinity.error().message());

    auto off_curve = full->bytes();
    off_curve[64] ^= byte(1);  // the last bit of Y
    println("{}", crypto::p256::public_key::from_bytes(off_curve).error().message());
}
```

Output:

```text
true
sgcl::crypto::p256: the point at infinity is not a public key
sgcl::crypto::p256: the point is not on the curve
```

## See also

- [bytes](bytes.md), [bytes_compressed](bytes_compressed.md): the two forms
- [from_pkix_der](from_pkix_der.md): a key in a SubjectPublicKeyInfo
- [sgcl::crypto::p256::public_key](../p256-public_key.md)
