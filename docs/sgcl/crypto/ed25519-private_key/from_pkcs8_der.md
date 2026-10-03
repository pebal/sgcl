[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::from_pkcs8_der

```cpp
static expected<private_key, error> from_pkcs8_der(const slice<const byte>& der) noexcept;
```

Makes the key of a PKCS #8 PrivateKeyInfo (RFC 8410, OID 1.3.101.112), the seed inside, the DER that OpenSSL writes
and Go's `x509.ParsePKCS8PrivateKey` reads. The reading is strict DER. A version 1 PKCS #8 (RFC 5958's
OneAsymmetricKey) may carry the public key, which must be the one the seed gives, and attributes, which are
skipped. The seed goes from `der` into the object, its copy on the stack zeroed; nothing of it is left in managed
memory.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the PrivateKeyInfo |

## Return value

The key, or an [error](../error.md): `errc::unsupported` for another algorithm's key, `errc::malformed` for DER that
cannot be read, with the [offset](../error/offset.md) where it was found, and `errc::invalid_key` for a public key
that is not the seed's.

## Complexity

Linear in the size of `der`, and one SHA-512 and one fixed-base multiplication.

## Exceptions

None.

## Notes

The DER holds the seed: a program keeps it in a [secret_bytes](../secret_bytes.md), as
[to_pkcs8_der](to_pkcs8_der.md) gives it, and reads a file of it with [read_secret](../read_secret.md).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 seed in a PKCS #8
    auto der = encoding::hex::decode(
        "302e020100300506032b657004220420"
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    auto key = crypto::ed25519::private_key::from_pkcs8_der(der);
    println("{}", encoding::hex::encode(key->public_key().bytes()));

    // the same seed under X25519's OID, 1.3.101.110
    auto other = encoding::hex::decode(
        "302e020100300506032b656e04220420"
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    println("{}", crypto::ed25519::private_key::from_pkcs8_der(other).error().message());
}
```

Output:

```text
d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a
offset 9: the key is of another algorithm
```

## See also

- [to_pkcs8_der](to_pkcs8_der.md): the reverse
- [from_pem](from_pem.md): the key of PEM text
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
