[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::basic_auth

```cpp
#include "sgcl/net/http/auth.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class basic_auth;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Basic authentication of a server (RFC 7617), a [middleware](../middleware.md): a request whose
`Authorization: Basic` the program's verify takes goes on to the handler, its user the request's
[authenticated_user](../request/authenticated_user.md); any other request is answered 401 with `WWW-Authenticate: Basic realm="...",
charset="UTF-8"`, which makes a browser ask for a name and a password. Go's standard library reads the field
(`r.BasicAuth`) and leaves the rest to the handler; this is the rest.

## Rules

- The password crosses the network as it is typed (base64 is no encryption): over https alone, or
  [digest_auth](../digest_auth/README.md).
- verify runs on the worker, as a plain handler would; a slow hash of the password (bcrypt, Argon2) is the program's
  to call there. A comparison of its own is best made in constant time (`crypto::constant_time::equal`).
- A handle of one word: copies share the realm and the function.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](basic_auth.md) | the middleware of a realm and a verify function |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::basic_auth("admin", [](const string& user, const string& password) {
        return user == "ann" && password == "open sesame";
    }));
    srv.route("GET /panel", [](net::http::request req, net::http::response_writer w) {
        w.write("hello, " + req.authenticated_user() + "\n");
    });

    net::http::response_recorder refused;
    refused.serve(srv, net::http::test_request("GET", "/panel"));
    println("{} {}", refused.status(), refused.header("WWW-Authenticate"));

    auto req = net::http::test_request("GET", "/panel");
    req.set_basic_auth("ann", "open sesame");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    print("{} {}", rec.status(), rec.body());
}
```

Output:

```text
401 Basic realm="admin", charset="UTF-8"
200 hello, ann
```

## See also

- [digest_auth](../digest_auth/README.md): the password never sent
- [request::set_basic_auth](../request/set_basic_auth.md), [request::basic_auth](../request/basic_auth.md)
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
