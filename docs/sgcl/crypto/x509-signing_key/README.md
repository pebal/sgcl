[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::signing_key

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class signing_key;
}
```

`sgcl::crypto::x509::signing_key` is the private key a certificate or a certificate request is signed with: a view of
one of the module's five kinds of key, [p256](../p256-private_key/README.md), [p384](../p256-private_key/README.md),
[p521](../p256-private_key/README.md), [ed25519](../ed25519-private_key/README.md) and
[rsa](../rsa-private_key/README.md), made from the key itself where [create_certificate](../x509-create_certificate.md)
and [create_certificate_request](../x509-create_certificate_request.md) take one. Go passes a `crypto.Signer`, an
interface value; here the view is two words, the kind and the address of the key, and nothing is copied: the keys are
move-only and their bytes stay where they are.

What each kind signs with is what Go and OpenSSL sign with by default: ECDSA over SHA-256 for P-256, SHA-384 for P-384
and SHA-512 for P-521 (the signature in DER), Ed25519 over the bytes themselves, RSA PKCS #1 v1.5 over SHA-256.

## Rules

- **A view.** The key it is made from must outlive it, as a string must outlive a `std::string_view` of it: it is a
  parameter, made where it is passed, and never kept.
- **Made implicitly** from a key of any of the five kinds: `create_certificate(t, key)` takes the key itself.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](x509-signing_key.md) | a view of a key of one of the five kinds |
| [kind](kind.md) | the kind of the key |
| [public_key_der](public_key_der.md) | the SubjectPublicKeyInfo of the key's public half |

## See also

- [create_certificate](../x509-create_certificate.md), [create_certificate_request](../x509-create_certificate_request.md): what it signs
- [x509](../x509.md)
