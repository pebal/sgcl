[sgcl](../README.md) › [crypto](README.md) › [ed25519](ed25519.md)

# sgcl::crypto::ed25519::public_key

```cpp
#include "sgcl/crypto/ed25519.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::ed25519 {
    class public_key;
}
```

`sgcl::crypto::ed25519::public_key` is an Ed25519 key that verifies: a point of the curve, kept both as its 32 bytes
and decompressed, negated as the verification uses it, so that a key verifying many signatures decodes its point
once. It is made from its bytes by [from_bytes](ed25519-public_key/from_bytes.md), which takes only the canonical
encoding of a point (RFC 8032 §5.1.3), from a SubjectPublicKeyInfo by
[from_pkix_der](ed25519-public_key/from_pkix_der.md), or taken from a private key's
[public_key](ed25519-private_key/public_key.md); there is no public constructor but the copy. Go's
`ed25519.PublicKey` is a slice of 32 bytes, decoded at every verification.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- A plain value: copied and assigned freely, it lives anywhere. A public key is not a secret.

## Member functions

#### Conversions

| Function | Description |
|---|---|
| [from_bytes](ed25519-public_key/from_bytes.md) | the key of 32 bytes, a canonical point (static) |
| [from_pkix_der](ed25519-public_key/from_pkix_der.md) | the key of a SubjectPublicKeyInfo (static) |
| [to_pkix_der](ed25519-public_key/to_pkix_der.md) | the SubjectPublicKeyInfo, 44 bytes |

#### Observers

| Function | Description |
|---|---|
| [bytes](ed25519-public_key/bytes.md) | the 32 bytes |

#### Signatures

| Function | Description |
|---|---|
| [verify](ed25519-public_key/verify.md) | checks whether a signature is this key's signature of a message |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](ed25519-public_key/operator_cmp.md) | compares the bytes of two keys |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1: the public key, the empty message and its signature
    auto key = crypto::ed25519::public_key::from_bytes(
        encoding::hex::decode("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"));
    auto signature = encoding::hex::decode(
        "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    println("{}", key->verify("", signature));
    println("{}", key->verify("a", signature));
    println("{} bytes", key->to_pkix_der().size());
}
```

Output:

```text
true
false
44 bytes
```

## See also

- [ed25519::private_key](ed25519-private_key.md): the key that signs
- [sgcl::crypto::ed25519](ed25519.md)
