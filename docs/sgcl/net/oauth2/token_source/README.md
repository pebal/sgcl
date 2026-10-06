[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md)

# sgcl::net::oauth2::token_source

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    class token_source;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Valid tokens of a [config](../config/README.md), Go's `oauth2.TokenSource` with its `ReuseTokenSource`: the current
token while it is [valid](../token/valid.md), a new one when it is not — of its refresh token, or of the client's
credentials. However many tasks ask at once, one refresh runs and the others wait for it and take its token. Its
[client](client.md) is an [http::client](../../http/client/README.md) that sends the token with every request.

## Rules

- Made by [config::source](../config/source.md).
- A handle of one word: copies share the token, and any number of tasks and threads may use one.
- A refresh token the server rotates is kept: the next refresh uses the new one.

## Member functions

| Function | Description |
|---|---|
| [token](token.md) | the token now, refreshed when it expired |
| [async_refresh](async_refresh.md) | the token refreshed now |
| [client](client.md) | an http::client that sends the tokens |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/oauth2.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;  // the authorization server and the resource, here in the program
    srv.route("POST /token", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"access_token":"at-1","expires_in":3600})");
    });
    srv.route("GET /api/me", [](net::http::request req, net::http::response_writer w) {
        w.write("you sent " + req.header("Authorization"));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    net::http::client api = cfg.source().client();
    println("{}", api.get(base + "/api/me")->text().value());
    srv.close();
}
```

Output:

```text
you sent Bearer at-1
```

## See also

- [config::source](../config/source.md)
- [token](../token/README.md)
