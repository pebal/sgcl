[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md)

# sgcl::net::acme::account_key

```cpp
#include "sgcl/net/acme/key.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    class account_key;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::acme::account_key` is the key of an ACME account: what every request of a [client](../client/README.md) is
signed with (a JSON Web Signature, RFC 7515, in the flattened form of RFC 8555 §6.2), and what the CA knows the account
by. ES256 over P-256 by default, ES384, EdDSA (RFC 8037) where the CA takes it, RS256 for an existing RSA key. Its
public half as a JWK (RFC 7517) and the JWK's thumbprint (RFC 7638), which every challenge's key authorization is made
of. Go passes a `crypto.Signer`; here one handle holds the key of any of the four kinds.

## Rules

- **A handle of one word.** Its copies are the same key, safe from many threads; the private key lives in an
  unmanaged block of the handle's state, never copied, zeroed by its own destructor when the state is collected.
- **Made with its key**: a new one by the constructor, one of a PEM by [from_pem](from_pem.md); kept with
  [to_pem](to_pem.md) as a `secret_bytes`, for a file of 0600. The [manager](../manager/README.md) keeps its own in its cache.
- **The JWK and the thumbprint are made once**, with the key.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](account_key.md) | a new key, P-256 or of an algorithm |
| [from_pem](from_pem.md) | a key of a PEM text (static) |
| [to_pem](to_pem.md) | the key as PEM, PKCS #8 |
| [algorithm](algorithm.md) | the JWS algorithm it signs with |
| [jwk](jwk.md) | the public key as a JWK |
| [thumbprint](thumbprint.md) | the JWK thumbprint |
| [key_authorization](key_authorization.md) | the key authorization of a challenge's token |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the keys |

## See also

- [client](../client/README.md): what signs with it
- [key_algorithm](../key_algorithm.md)
- [net::acme](../README.md)
