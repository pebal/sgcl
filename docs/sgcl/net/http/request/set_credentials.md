[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::set_credentials

```cpp
request& set_credentials(const string& user, const string& password) noexcept;
```

Gives the request credentials to answer a 401 of its origin with: once, Digest when the server offers it (the strongest
algorithm of SHA-512/256, SHA-256 and MD5), else Basic, the protection space then remembered by the client for the next
requests (see [client](../client/README.md)). They win over the client's [credentials](../credentials.md). Nothing is
sent before the server asks, nor to another origin a redirect leads to. A body is sent again for the answer, so a request
with a stream body gets the 401.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `password` | the password |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::digest_auth("files", [](const string& u) -> optional<string> {
        if (u == "ann") {
            return string("secret");
        }
        return nullopt;
    }));
    srv.route("GET /me", [](net::http::request req, net::http::response_writer w) { w.write(req.authenticated_user()); });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    net::http::request req("GET", "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/me");
    req.set_credentials("ann", "secret");
    net::http::client web;
    println("{}", web.send(req)->text().value());
    srv.close();
}
```

Output:

```text
ann
```

## See also

- [credentials](../credentials.md)
- [set_basic_auth](set_basic_auth.md)
- [request](README.md)
