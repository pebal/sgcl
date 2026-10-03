[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](../p256-public_key.md)

# sgcl::crypto::p256::public_key::to_pkix_der

```cpp
vector<byte> to_pkix_der() const noexcept;
```

The key as a SubjectPublicKeyInfo (RFC 5280 §4.1.2.7, RFC 5480): `id-ecPublicKey` with the named curve and the
uncompressed point, byte for byte as Go's `x509.MarshalPKIXPublicKey` and OpenSSL write it. It is the DER of a
`PUBLIC KEY` block in PEM, which [encoding::pem](../../encoding/pem.md) writes, and what
[from_pkix_der](from_pkix_der.md) reads: 91 bytes on P-256, 120 on P-384.

## Parameters

None.

## Return value

The DER of the SubjectPublicKeyInfo, in a new vector.

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
    auto der = key->to_pkix_der();
    println("{} bytes", der.size());
    println("{}", encoding::hex::encode(der.as_slice(0, 27)));  // everything before X
    println("{}", crypto::p256::public_key::from_pkix_der(der) == key);
}
```

Output:

```text
91 bytes
3059301306072a8648ce3d020106082a8648ce3d03010703420004
true
```

## See also

- [from_pkix_der](from_pkix_der.md): the key of a SubjectPublicKeyInfo
- [bytes](bytes.md): the point alone
- [sgcl::crypto::p256::public_key](../p256-public_key.md)
