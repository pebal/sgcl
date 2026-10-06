[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::authorization_url

```cpp
string authorization_url(const string& state, const pkce& p,
                         const vector<pair<string, string>>& extra = {}) const noexcept;
```

The URL of the authorization request (RFC 6749 §4.1.1) the user's browser is sent to: the `authorization` endpoint
with `response_type=code`, the client's id, the redirect, the scopes, the state, the PKCE challenge with
`code_challenge_method=S256` (none for a `pkce` left empty), then the extra parameters — OpenID Connect's `nonce`, a
`prompt`, a server's own — in their order. The server sends the user back to the redirect with `code` and the same
`state`, which the program checks against the one it gave before it calls [exchange](exchange.md).

## Parameters

| Parameter | Description |
|---|---|
| `state` | an unguessable value of the user's session, that ties the redirect to it (CSRF) |
| `p` | the PKCE pair of this request ([pkce::generate](../pkce/generate.md)) |
| `extra` | more parameters |

## Return value

The URL.

## Complexity

Linear in the length of the parameters.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"

using namespace sgcl;

int main() {
    net::oauth2::config cfg;
    cfg.client_id = "photos";
    cfg.endpoints.authorization = "https://as.example/authorize";
    cfg.redirect_url = "http://127.0.0.1:8080/callback";
    cfg.scopes = {"read", "write"};
    println("{}", cfg.authorization_url("st-1", {}, {{"prompt", "consent"}}));
}
```

Output:

```text
https://as.example/authorize?response_type=code&client_id=photos&redirect_uri=http%3A%2F%2F127.0.0.1%3A8080%2Fcallback&scope=read+write&state=st-1&prompt=consent
```

## See also

- [exchange](exchange.md)
- [pkce](../pkce/README.md)
- [config](README.md)
