[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [token_source](README.md)

# sgcl::net::oauth2::token_source::client

```cpp
http::client client() const;                            // (1)
http::client client(const http::client& base) const;    // (2)
```

An [http::client](../../http/client/README.md) that sends the source's token, Go's `Config.Client`: every request to the
origin of its own URL gets `Authorization: Bearer` of a [valid](../token/valid.md) token — refreshed first when it
expired — unless it carries an `Authorization` of its own; a redirect to another origin goes without it. A 401 whose
`WWW-Authenticate` says `error="invalid_token"` (RFC 6750 §3.1) refreshes the token and sends the request once more,
when its body can be sent again. A failed refresh is the request's error, the [error](../error/README.md)'s message in an
[io::error](../../../io/error/README.md) of `permission_denied` (or its transport error).

1. A copy of the config's `http` client: its settings, its pool.
2. A copy of the client given.

## Parameters

| Parameter | Description |
|---|---|
| `base` | the client whose settings and pool the new one shares |

## Return value

The client.

## Complexity

Constant.

## Exceptions

None.

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
        w.write(R"({"access_token":"at-2","expires_in":3600})");
    });
    srv.route("GET /api/me", [](net::http::request req, net::http::response_writer w) {
        if (req.header("Authorization") != "Bearer at-2") {
            w.set_header("WWW-Authenticate", R"(Bearer error="invalid_token")");
            w.set_status(401);
            return;
        }
        w.write("ann");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    net::oauth2::token revoked;  // still valid to the client, refused by the resource
    revoked.access_token = "at-1";
    revoked.refresh_token = "rt-1";
    net::http::client api = cfg.source(revoked).client();
    println("{}", api.get(base + "/api/me")->text().value());
    srv.close();
}
```

Output:

```text
ann
```

## See also

- [http::client](../../http/client/README.md)
- [async_refresh](async_refresh.md)
- [token_source](README.md)
