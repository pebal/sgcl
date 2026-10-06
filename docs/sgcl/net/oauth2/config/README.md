[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md)

# sgcl::net::oauth2::config

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    class config;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

The client of one authorization server, Go's `oauth2.Config`: a value whose fields are set by name, and on it every
call of the protocol — the authorization code grant with PKCE, the client credentials grant, the device authorization
grant, refresh, revocation and introspection — and the [token sources](../token_source/README.md) that keep tokens
valid by themselves.

## Rules

- A value: a copy is another config with the same settings, and its `http` member the same client (its pool).
- Every request is a POST of a form to an endpoint, with the client authenticated as `auth` says, and its answer a JSON
  object; the redirect of the authorization code grant is the program's to receive (a route of its server, or the
  loopback of RFC 8252 for a desktop program).

## Member objects

| Member | Description |
|---|---|
| `client_id` | the client's id |
| `client_secret` | its secret; `""` for a public client |
| `endpoints` | the server's URLs ([endpoints](../endpoints.md)) |
| `redirect_url` | where the server sends the user back with the code |
| `scopes` | `vector<string>`: the scopes asked for |
| `auth` | how the client proves itself ([client_auth](../client_auth.md)); `client_auth::basic` |
| `http` | the [http::client](../../http/client/README.md) of the endpoints: its TLS, proxy, timeouts |

## Member functions

| Function | Description |
|---|---|
| [authorization_url](authorization_url.md) | the URL the user is sent to |
| [exchange](exchange.md) | the code of the redirect exchanged for a token |
| [client_credentials](client_credentials.md) | the client's own token |
| [device_authorize](device_authorize.md) | the codes of the device flow |
| [device_token](device_token.md) | the device flow's token, polled for |
| [refresh](refresh.md) | a new access token of a refresh token |
| [revoke](revoke.md) | a token revoked |
| [introspect](introspect.md) | what the server says of a token |
| [source](source.md) | a source of valid tokens |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/oauth2.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server as;  // the authorization server, here in the program
    as.route("POST /token", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"access_token":"at-1","token_type":"Bearer",)"
                R"("expires_in":3600,"refresh_token":"rt-1"})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.client_id = "reports";
    cfg.client_secret = "s3cret";
    cfg.endpoints.token = base + "/token";
    cfg.scopes = {"read"};
    auto t = cfg.client_credentials();
    println("{} {}", t->access_token, t->valid());
    as.close();
}
```

Output:

```text
at-1 true
```

## See also

- [token_source](../token_source/README.md)
- [token](../token/README.md)
