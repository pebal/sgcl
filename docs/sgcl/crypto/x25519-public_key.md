[sgcl](../README.md) › [crypto](README.md) › [x25519](x25519.md)

# sgcl::crypto::x25519::public_key

```cpp
#include "sgcl/crypto/x25519.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x25519 {
    class public_key;
}
```

`sgcl::crypto::x25519::public_key` is a peer's X25519 key: the u coordinate of a point of Curve25519, 32 bytes, what
one side of a key exchange sends the other. Any 32 bytes are one (RFC 7748 §5): bit 255 is ignored and a value of p
or more is reduced, as the RFC and Go have it. It is made from its bytes by
[from_bytes](x25519-public_key/from_bytes.md), from a SubjectPublicKeyInfo by
[from_pkix_der](x25519-public_key/from_pkix_der.md), or taken from a private key's
[public_key](x25519-private_key/public_key.md); there is no public constructor but the copy. Go's `*ecdh.PublicKey`
is a pointer; this is the 32 bytes themselves.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- A plain value of 32 bytes: trivially copyable, copied and assigned freely, it lives anywhere. A public key is not
  a secret.

## Member functions

#### Conversions

| Function | Description |
|---|---|
| [from_bytes](x25519-public_key/from_bytes.md) | the key of 32 bytes (static) |
| [from_pkix_der](x25519-public_key/from_pkix_der.md) | the key of a SubjectPublicKeyInfo (static) |
| [to_pkix_der](x25519-public_key/to_pkix_der.md) | the SubjectPublicKeyInfo, 44 bytes |

#### Observers

| Function | Description |
|---|---|
| [bytes](x25519-public_key/bytes.md) | the 32 bytes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](x25519-public_key/operator_cmp.md) | compares the bytes of two keys |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1: Bob's public key, as Alice receives it
    auto bob = crypto::x25519::public_key::from_bytes(
        encoding::hex::decode("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f"));
    auto der = bob->to_pkix_der();
    println("{} bytes: {}", der.size(), encoding::hex::encode(der));

    auto back = crypto::x25519::public_key::from_pkix_der(der);
    println("{}", back == bob);
}
```

Output:

```text
44 bytes: 302a300506032b656e032100de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f
true
```

## See also

- [x25519::private_key](x25519-private_key.md): the key that computes the shared secret
- [sgcl::crypto::x25519](x25519.md)
