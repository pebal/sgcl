[sgcl](../../README.md) › [net](../README.md) › oauth2

# sgcl::net::oauth2

```cpp
#include "sgcl/net/oauth2.h"   // namespace sgcl::net::oauth2
```

OAuth 2.0 from the client's side (RFC 6749 and 6750): Go's `golang.org/x/oauth2` in one namespace. A
[config](config/README.md) is the client of one authorization server — its id and secret, the
[endpoints](endpoints.md), the redirect, the scopes — and every grant is a call on it: the authorization code with PKCE
(RFC 7636), the client's own credentials, the device authorization of RFC 8628 for a device without a browser,
refresh; a token is revoked (RFC 7009) and inspected (RFC 7662) the same way.

The idea it rests on is that the client does not hold tokens, a source does. A [token](token/README.md) is a value, what
the server said when it was asked; a [token_source](token_source/README.md) keeps the current one and refreshes it when
it expires, one refresh at a time however many tasks ask, and its [client](token_source/client.md) is an
[http::client](../http/client/README.md) that sends the token with every request to the resource and answers a 401 of
`invalid_token` by refreshing it and sending the request once more. A program writes its requests as it always does;
the tokens take care of themselves.

## The rules

1. Every call that talks to the server has the two forms of the module ([The rules](../README.md#the-rules), 2): `x`
   blocks the thread, `async_x` returns a task.
2. A failure is an [error](error/README.md): the server's own (RFC 6749 §5.2, its code `invalid_grant`,
   `authorization_pending`, …) or one of the exchange, its [io::error](../../io/error/README.md) in `transport()`.
3. A token is valid until 10 seconds before it expires, as in Go: one that dies on its way is no use.
4. The client of the server's endpoints is the config's `http` member: its TLS, its proxy, its timeouts.

## Classes

| Class | Header | Description |
|---|---|---|
| [client_auth](client_auth.md) | `oauth2/oauth2.h` | how the client proves itself to the token endpoint: Basic, the form, none |
| [config](config/README.md) | `oauth2/oauth2.h` | the client of an authorization server: the grants, refresh, revocation, introspection |
| [device_authorization](device_authorization.md) | `oauth2/oauth2.h` | what a device shows its user and polls with (RFC 8628) |
| [endpoints](endpoints.md) | `oauth2/oauth2.h` | the server's URLs |
| [error](error/README.md) | `oauth2/oauth2.h` | an error of the protocol, or of the exchange |
| [introspection](introspection.md) | `oauth2/oauth2.h` | what the server says of a token (RFC 7662) |
| [pkce](pkce/README.md) | `oauth2/oauth2.h` | a PKCE verifier and its S256 challenge (RFC 7636) |
| [token](token/README.md) | `oauth2/oauth2.h` | a token of the token endpoint |
| [token_source](token_source/README.md) | `oauth2/oauth2.h` | valid tokens, refreshed when they expire, and a client that sends them |

## See also

- [http::client](../http/client/README.md)
- [http::basic_auth](../http/basic_auth/README.md)
- [net](../README.md)
