[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::from_pem

```cpp
static expected<jwk, error> from_pem(const string& pem) noexcept;
static expected<jwk, error> from_pem(const secret_bytes& pem) noexcept;
```

The private key of the first key block of a PEM text, as the keys' own `from_pem` read it: PKCS #8 (`PRIVATE KEY`)
of P-256, P-384, RSA, Ed25519 or X25519, SEC 1 (`EC PRIVATE KEY`), PKCS #1 (`RSA PRIVATE KEY`). Its base64 is
decoded straight into plain memory; the text may be the [secret_bytes](../secret_bytes/README.md) of
[read_secret](../read_secret.md). The key has no `kid`, `alg` or `use`.

## Parameters

| Parameter | Description |
|---|---|
| `pem` | the PEM text: a string, or a secret_bytes |

## Return value

The key, or an error: `errc::malformed` for text without a key block, `errc::unsupported` for an encrypted key or a
key of a kind the module does not have.

## Complexity

Linear in the length of the text, and the check of the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::p256::private_key::generate().to_pem();
    auto key = crypto::jose::jwk::from_pem(pem);
    println("{} {}", key->crv(), key->is_private());
}
```

Output:

```text
P-256 true
```

## See also

- [to_pem](to_pem.md): the other way
- [sgcl::crypto::jose::jwk](README.md)
