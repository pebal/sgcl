[sgcl](../../README.md) › [net](../README.md) › oidc

# sgcl::net::oidc

```cpp
#include "sgcl/net/oidc.h"   // namespace sgcl::net::oidc
```

OpenID Connect (Core 1.0, Discovery 1.0) on [net::oauth2](../oauth2/README.md): signing a user in with an account of
another service. A [provider](provider/README.md) is found by its issuer — its endpoints and its keys read from the
discovery document — and gives the [oauth2::config](../oauth2/config/README.md) of the authorization code grant; the
token response's ID token is then [verified](provider/verify.md) with [crypto::jose](../../crypto/jose.md) and read as
an [id_token](id_token.md): who the user is, for which client, until when. Go's `coreos/go-oidc` in one namespace.

The idea it rests on is that nothing in an ID token is believed before its signature and its claims are checked: the
signature by a key of the provider's own set (fetched again when the provider rotates its keys), the issuer the one
discovered, the audience the client, the expiry, and the nonce the program put in the authorization request
([request_secrets](request_secrets/README.md)), so that a token taken from someone else's sign-in is of no use.

## The rules

1. Every call that talks to the provider has the two forms of the module ([The rules](../README.md#the-rules), 2).
2. A failure is an [oauth2::error](../oauth2/error/README.md): the provider's, the exchange's in its `transport()`, or
   the module's own codes — `invalid_issuer` (the discovery document names another issuer), `invalid_metadata` (no
   `jwks_uri`), `invalid_jwks`, `invalid_id_token`, `invalid_userinfo`.

## Classes

| Class | Header | Description |
|---|---|---|
| [id_token](id_token.md) | `oidc/oidc.h` | an ID token verified: the issuer, the subject, the audience, the times, the nonce, the claims |
| [provider](provider/README.md) | `oidc/oidc.h` | an OpenID provider: discovery, ID tokens verified, userinfo |
| [provider::verify_options](provider-verify_options.md) | `oidc/oidc.h` | the client, the nonce and the leeway an ID token is checked with |
| [request_secrets](request_secrets/README.md) | `oidc/oidc.h` | the state and the nonce of one authorization request |

## See also

- [net::oauth2](../oauth2/README.md)
- [crypto::jose](../../crypto/jose.md)
- [net](../README.md)
