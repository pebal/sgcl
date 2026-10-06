[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::authenticated_user

```cpp
string authenticated_user() const noexcept;
```

The user an authentication middleware of the server ([basic_auth](../basic_auth/README.md),
[digest_auth](../digest_auth/README.md)) let the request in as; `""` when none ran, or for a request a program made.

## Parameters

None.

## Return value

The user, or `""`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::basic_auth("x", [](const string& u, const string& p) { return p == "pw"; }));
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) { w.write("[" + req.authenticated_user() + "]"); });
    auto req = net::http::test_request("GET", "/");
    req.set_basic_auth("eve", "pw");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    println("{} {}", rec.body(), "[" + net::http::test_request("GET", "/").authenticated_user() + "]");
}
```

Output:

```text
[eve] []
```

## See also

- [basic_auth](basic_auth.md)
- [request](README.md)
