[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::generate

```cpp
static jwk generate(algorithm a);
static jwk generate(algorithm a, const options& o);
```

A new key fit for `a`, whose `alg` it becomes unless `o` names another: random octets of the digest's length for HS*
(32, 48, 64 bytes), RSA of 2048 bits for RS*, PS* and RSA-OAEP*, P-256 for ES256 and ECDH-ES, P-384 for ES384, P-521 for ES512,
Ed25519 for EdDSA, random octets of 16, 24 or 32 bytes for the AES key wraps.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the algorithm the key is for |
| `o` | `kid`, `alg` and `use` ([options](../jose-jwk-options.md)) |

## Return value

The key.

## Complexity

Constant; an RSA key takes the time of finding two primes (tens of milliseconds).

## Exceptions

`std::invalid_argument` for `dir`, which has no key of its own (a [symmetric](symmetric.md) key of the content
encryption's length is one), and for a value of no algorithm.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::es384, {.kid = "k"});
    println("{} {}", key.crv(), key.parameters()["alg"].as_string("?"));
}
```

Output:

```text
P-384 ES384
```

## See also

- [(constructor)](jose-jwk.md): a key the program has
- [sgcl::crypto::jose::jwk](README.md)
