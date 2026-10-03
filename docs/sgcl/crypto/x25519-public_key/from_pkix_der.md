[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [public_key](README.md)

# sgcl::crypto::x25519::public_key::from_pkix_der

```cpp
static expected<public_key, error> from_pkix_der(const slice<const byte>& der) noexcept;
```

Makes the key of a SubjectPublicKeyInfo (RFC 8410, OID 1.3.101.110), the DER that OpenSSL writes and Go's
`x509.ParsePKIXPublicKey` reads. The reading is strict DER: nothing before, inside or after the structure but what
it must hold.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the SubjectPublicKeyInfo |

## Return value

The key, or an [error](../error/README.md): `errc::unsupported` for another algorithm's key, `errc::malformed` for DER that
cannot be read and `errc::invalid_key` for a key that is not 32 bytes, each with the [offset](../error/offset.md) of
the byte where it was found.

## Complexity

Linear in the size of `der`.

## Exceptions

None.

## Notes

The DER of a public key is not a secret, so a PEM block of one is read by [encoding::pem](../../encoding/pem/README.md)
and its bytes given here.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1: Alice's public key in a SubjectPublicKeyInfo
    auto der = encoding::hex::decode(
        "302a300506032b656e0321008520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    auto alice = crypto::x25519::public_key::from_pkix_der(der);
    println("{}", encoding::hex::encode(alice->bytes()));

    // the same bytes under Ed25519's OID, 1.3.101.112
    auto other = encoding::hex::decode(
        "302a300506032b6570032100d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    println("{}", crypto::x25519::public_key::from_pkix_der(other).error().message());
}
```

Output:

```text
8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a
offset 6: the key is of another algorithm
```

## See also

- [to_pkix_der](to_pkix_der.md): the reverse
- [from_bytes](from_bytes.md): the key of 32 bytes
- [sgcl::crypto::x25519::public_key](README.md)
