[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::thumbprint

```cpp
string thumbprint() const;             // (1)
string thumbprint(hash_id h) const;    // (2)
```

The JWK thumbprint (RFC 7638): the key's required members in lexicographic order without white space (`crv`, `kty`,
`x`, `y` of an EC key; `e`, `kty`, `n` of RSA; `crv`, `kty`, `x` of OKP; `k`, `kty` of oct), hashed, in base64url. A
name of the key that does not depend on how its JSON was written, and the same for a private key and its public half:
ACME's key authorizations, DPoP's `jkt`, a `kid` made from the key.

1. By SHA-256, as RFC 7638 and its users have it.
2. By the digest `h` names.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the digest |

## Return value

The thumbprint in base64url without padding: 43 characters of SHA-256.

## Complexity

Linear in the size of the key.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` for a `hash_id` of no value of its enumeration.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8037 A.3: the thumbprint of A.2's key
    auto key = crypto::jose::jwk::parse(
        R"({"kty":"OKP","crv":"Ed25519","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})");
    println("{}", key->thumbprint());
}
```

Output:

```text
kPrK_qmxVWaYVA9wwBF6Iuo3vVzz7TxHCTwXBygrS4k
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::crypto::jose::jwk](README.md)
