[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::credentials

```cpp
#include "sgcl/net/http/client.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    struct credentials {
        string user;
        string password;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::credentials` is a user and a password a [client](client/README.md) answers a 401's challenge with: the
client's member `credentials`, for the requests to the origin of each request's own URL, or a request's own
([set_credentials](request/set_credentials.md)). The answer is Digest when the server offers it (the strongest algorithm
of SHA-512/256, SHA-256 and MD5), else Basic; the password is never sent before the server asks, nor to another origin a
redirect leads to.

## Member objects

| Member | Description |
|---|---|
| `user` | the user's name, as UTF-8 |
| `password` | the password, as UTF-8 |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::basic_auth("api", [](const string& u, const string& p) { return u == "bot" && p == "t0k"; }));
    srv.route("GET /status", [](net::http::request req, net::http::response_writer w) { w.write("ok for " + req.authenticated_user()); });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::http::client web;
    web.credentials = net::http::credentials{"bot", "t0k"};
    auto res = web.get("http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/status");
    println("{}", res->text().value());
    srv.close();
}
```

Output:

```text
ok for bot
```

## See also

- [client](client/README.md)
- [request::set_credentials](request/set_credentials.md)
- [sgcl::net::http](README.md)
