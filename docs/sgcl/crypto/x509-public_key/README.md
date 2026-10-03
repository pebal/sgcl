[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::public_key

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class public_key;
}
```

`sgcl::crypto::x509::public_key` is the public key of a [certificate](../x509-certificate/README.md) as one of the module's key
types, or none: [kind](kind.md) says which, and [rsa](rsa.md),
[p256](p256.md), [p384](p384.md) and [ed25519](ed25519.md) give it.
[algorithm](algorithm.md) is the OID of the SubjectPublicKeyInfo whatever the kind.

Go's `Certificate.PublicKey` is an `any` that a program asserts to a type; here the kinds are a closed list, a
`switch` over `kind()`, or a `visit` of [value](value.md).

## Rules

- **None is a kind.** A key of an algorithm the module has no type for (DSA, X25519, P-521, ML-DSA), a key its type
  refuses (an RSA key under 1024 bits, a point off the curve), or an EC key whose parameters are explicit or NULL
  rather than a named curve is `key_kind::none`, and the certificate is still read. A certificate signed by such a key
  does not verify (`unsupported_algorithm`).
- **The wrong kind is the program's mistake**: `rsa()` of a key that is not RSA is `logic_error`, as the others are.
- It holds a [string](../../core/string/README.md), so it lives where a `tracked_ptr` may.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `variant<monostate, crypto::rsa::public_key, crypto::p256::public_key, crypto::p384::public_key, crypto::ed25519::public_key>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](x509-public_key.md) | no key |

#### Observers

| Function | Description |
|---|---|
| [kind](kind.md) | which key it is |
| [has_value](has_value.md) | checks whether it is a key of the module |
| [rsa](rsa.md) | the RSA key |
| [p256](p256.md) | the P-256 key |
| [p384](p384.md) | the P-384 key |
| [ed25519](ed25519.md) | the Ed25519 key |
| [value](value.md) | the key as the variant |
| [algorithm](algorithm.md) | the OID of the key's algorithm |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* file : {"rsa.pem", "ecdsa.pem", "ed25519.pem"}) {
        auto text = io::read_text(string("tests/net/tls_testdata/") + file);
        crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);
        auto& key = cert.public_key();
        switch (key.kind()) {
            case crypto::x509::key_kind::rsa: println("RSA, {} bits", key.rsa().bits()); break;
            case crypto::x509::key_kind::p256: println("P-256"); break;
            case crypto::x509::key_kind::ed25519: println("Ed25519"); break;
            default: println("another key: {}", key.algorithm()); break;
        }
    }
}
```

Output:

```text
RSA, 2048 bits
P-256
Ed25519
```

## See also

- [key_kind](../x509-key_kind.md)
- [certificate::public_key](../x509-certificate/public_key.md)
- [sgcl::crypto::x509](../x509.md)
