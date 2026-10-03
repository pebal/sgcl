[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [public_key](../ed25519-public_key.md)

# sgcl::crypto::ed25519::public_key::from_pkix_der

```cpp
static expected<public_key, error> from_pkix_der(const slice<const byte>& der) noexcept;
```

Makes the key of a SubjectPublicKeyInfo (RFC 8410, OID 1.3.101.112), the DER that OpenSSL writes and Go's
`x509.ParsePKIXPublicKey` reads, and then takes its 32 bytes as [from_bytes](from_bytes.md) takes them. The reading
is strict DER.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the SubjectPublicKeyInfo |

## Return value

The key, or an [error](../error.md): `errc::unsupported` for another algorithm's key, `errc::malformed` for DER that
cannot be read, with the [offset](../error/offset.md) where it was found, and the errors of
[from_bytes](from_bytes.md).

## Complexity

Linear in the size of `der`, and one decompression.

## Exceptions

None.

## Notes

The DER of a public key is not a secret, so a PEM block of one is read by [encoding::pem](../../encoding/pem.md)
and its bytes given here.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 key in a PEM block, as OpenSSL writes it
    encoding::pem block("-----BEGIN PUBLIC KEY-----\n"
                        "MCowBQYDK2VwAyEA11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=\n"
                        "-----END PUBLIC KEY-----\n");
    auto key = crypto::ed25519::public_key::from_pkix_der(block.bytes());
    println("{}", encoding::hex::encode(key->bytes()));

    auto cut = crypto::ed25519::public_key::from_pkix_der(block.bytes().as_slice().subslice(0, 40));
    println("{}", cut.error().message());
}
```

Output:

```text
d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a
DER: not one SEQUENCE
```

## See also

- [to_pkix_der](to_pkix_der.md): the reverse
- [from_bytes](from_bytes.md): the key of 32 bytes
- [sgcl::crypto::ed25519::public_key](../ed25519-public_key.md)
