[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](README.md)

# sgcl::crypto::p256::public_key::from_pkix_der

```cpp
static expected<public_key, error> from_pkix_der(const slice<const byte>& data) noexcept;
```

Reads a SubjectPublicKeyInfo (RFC 5280 §4.1.2.7, RFC 5480) of this curve, Go's `x509.ParsePKIXPublicKey` for an
ECDSA key: the DER of a `PUBLIC KEY` block in PEM, the key of a certificate. The algorithm must be
`id-ecPublicKey` with the named curve P-256, and the key a point [from_bytes](from_bytes.md) accepts, uncompressed or
compressed. The DER is read strictly: lengths and integers in their shortest form, nothing after the structure.

`p384::public_key::from_pkix_der` reads the keys whose named curve is P-384.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the DER of the SubjectPublicKeyInfo |

## Return value

The key, or a [crypto::error](../error/README.md) with the offset of the byte where the reading stopped:

- `errc::malformed` for DER that is not a SubjectPublicKeyInfo;
- `errc::unsupported` for a key of another algorithm or of another curve;
- `errc::invalid_key` for a point `from_bytes` refuses.

## Complexity

Linear in the size of `data`, and the check of the point.

## Exceptions

None.

## Notes

The PEM around the DER is [encoding::pem](../../encoding/pem/README.md)'s: a public key is no secret, so its text may lie in
managed memory.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the SubjectPublicKeyInfo of the key of RFC 6979, A.2.5
    auto der = encoding::hex::decode(
        "3059301306072a8648ce3d020106082a8648ce3d03010703420004"
        "60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
        "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299");
    auto key = crypto::p256::public_key::from_pkix_der(der);
    println("{}", encoding::hex::encode(key->bytes_compressed()));

    // a key of P-384 is not one of P-256
    auto other = crypto::p384::private_key::generate().public_key().to_pkix_der();
    auto refused = crypto::p256::public_key::from_pkix_der(other);
    println("{}", refused.error().message());
    println("{}", refused.error().code() == crypto::errc::unsupported);
}
```

Output:

```text
0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6
offset 13: sgcl::crypto::p256: not a P-256 key
true
```

## See also

- [to_pkix_der](to_pkix_der.md): the SubjectPublicKeyInfo of a key
- [from_bytes](from_bytes.md): a point alone
- [sgcl::crypto::p256::public_key](README.md)
