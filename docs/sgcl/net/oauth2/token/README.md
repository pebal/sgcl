[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md)

# sgcl::net::oauth2::token

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    struct token;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A token of the token endpoint (RFC 6749 §5.1): the access token and its type, the refresh token, the scope the server
granted, OpenID Connect's ID token, when it expires and the whole response. A value: what the server said when it was
asked, never changed behind the program's back; a [token_source](../token_source/README.md) is what keeps a current
one. Go's `oauth2.Token`.

## Rules

- `expiry` is the moment the token came plus its `expires_in`; a token without one never expires to the client.
- A copy is another value with the same strings.

## Member objects

| Member | Description |
|---|---|
| `access_token` | what a request to the resource carries |
| `token_type` | `Bearer` by default; the server's when it sends one |
| `refresh_token` | what [refresh](../config/refresh.md) trades for a new token; `""` when the server gave none |
| `scope` | the scopes granted, separated by spaces; `""` when the server did not say (those asked for) |
| `id_token` | OpenID Connect's ID token, a JWT; `""` without OpenID |
| `expiry` | `optional<time::datetime>`: when it expires; nullopt when the server did not say |
| `raw` | `encoding::json`: the whole response, for the server's own members |

## Member functions

| Function | Description |
|---|---|
| [valid](valid.md) | whether the access token is there and 10 seconds from its expiry |

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
        w.write(R"({"access_token":"at-1","token_type":"bearer",)"
                R"("expires_in":3600,"refresh_token":"rt-1","scope":"read","tenant":"eu"})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    net::oauth2::token t = cfg.client_credentials().value();
    println("{} {} {} {}", t.access_token, t.token_type, t.refresh_token, t.scope);
    println("{} {}", t.valid(), t.raw["tenant"].as_string(""));
    as.close();
}
```

Output:

```text
at-1 bearer rt-1 read
true eu
```

## See also

- [token_source](../token_source/README.md)
- [config](../config/README.md)
