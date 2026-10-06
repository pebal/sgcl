[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [basic_auth](README.md)

# sgcl::net::http::basic_auth::basic_auth

```cpp
basic_auth(const string& realm, function<bool(const string& user, const string& password)> verify);
```

The middleware of a realm, the name a browser shows when it asks for the password, and of the function that says
whether a user and a password are right. The user and the password are what the client sent, the user before the first
colon and the password after it (RFC 7617 §2), as UTF-8.

## Parameters

| Parameter | Description |
|---|---|
| `realm` | the protection space's name, in the challenge |
| `verify` | `true` for a user and a password let in |

## Complexity

Constant.

## Exceptions

What the move of `verify` throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::basic_auth guard("reports", [](const string& user, const string& password) {
        return user == "auditor" && password == "2026";
    });
    net::http::server srv;
    srv.use(guard);
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) { w.write(req.authenticated_user()); });
    auto req = net::http::test_request("GET", "/");
    req.set_basic_auth("auditor", "2026");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    println("{}", rec.body());
}
```

Output:

```text
auditor
```

## See also

- [basic_auth](README.md)
