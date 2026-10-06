[sgcl](../../README.md) › [net](../README.md) › [oauth2](README.md)

# sgcl::net::oauth2::introspection

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    struct introspection {
        bool active = false;
        string scope;
        string client_id;
        string username;
        string token_type;
        string subject;
        string issuer;
        vector<string> audience;
        optional<time::datetime> expiry;
        optional<time::datetime> issued_at;
        optional<time::datetime> not_before;
        encoding::json raw;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What the introspection endpoint says of a token (RFC 7662 §2.2), [introspect](config/introspect.md)'s result. A token
the server does not know, or no longer honours, is `active` false and nothing else.

## Member objects

| Member | Description |
|---|---|
| `active` | whether the token is live |
| `scope` | its scopes, separated by spaces |
| `client_id` | the client it was issued to |
| `username` | the user's name, for people |
| `token_type` | `Bearer`, … |
| `subject` | `sub`: whom it stands for |
| `issuer` | `iss` |
| `audience` | `aud`: one string or an array, as a list |
| `expiry` | `exp`; nullopt when absent |
| `issued_at` | `iat`; nullopt when absent |
| `not_before` | `nbf`; nullopt when absent |
| `raw` | the whole response, for the server's own members |

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
    as.route("POST /introspect", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"active":true,"sub":"ann","aud":["api","admin"],"scope":"read"})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.introspection = base + "/introspect";
    net::oauth2::introspection info = cfg.introspect("at-1").value();
    println("{} {} {} {}", info.active, info.subject, info.audience.size(), info.scope);
    as.close();
}
```

Output:

```text
true ann 2 read
```

## See also

- [config::introspect](config/introspect.md)
