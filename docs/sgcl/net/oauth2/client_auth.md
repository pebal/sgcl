[sgcl](../../README.md) › [net](../README.md) › [oauth2](README.md)

# sgcl::net::oauth2::client_auth

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    enum class client_auth : uint8_t {
        basic,
        post,
        none,
    };
}
```

How a [config](config/README.md) proves the client to the token endpoint (RFC 6749 §2.3.1), its `auth` member: the
names of RFC 8414's `token_endpoint_auth_methods_supported` without their prefix.

| Value | Description |
|---|---|
| `basic` | `client_secret_basic`, the default: `Authorization: Basic` of the id and the secret, each form-urlencoded first |
| `post` | `client_secret_post`: `client_id` and `client_secret` in the form |
| `none` | a public client (an application in a browser or on a phone): `client_id` in the form alone; PKCE proves it |

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
        w.write(R"({"access_token":"at-1"})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.client_id = "spa";
    cfg.auth = net::oauth2::client_auth::none;
    cfg.endpoints.token = base + "/token";
    auto t = cfg.client_credentials();
    println("{}", t->access_token);
    as.close();
}
```

Output:

```text
at-1
```

## See also

- [config](config/README.md)
