[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [csrf](csrf/README.md) › tokens

# sgcl::net::http::csrf::tokens

```cpp
#include "sgcl/net/http/csrf.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class csrf {
    public:
        enum class tokens : uint8_t {
            none,
            synchronizer,
            double_submit,
        };
    };
}
```

The tokens a [csrf](csrf/README.md) asks an unsafe request to send back, beside the origin check, which runs in every
mode. A token is what [csrf::token](csrf/token.md) gave the page the request came from, sent in the options' field
(`X-CSRF-Token`) or a urlencoded form's field (`csrf_token`).

| Value | Description |
|---|---|
| `none` | no token: the origin check alone, Go's `CrossOriginProtection`. The default |
| `synchronizer` | a token of 256 random bits kept in the request's [session](session/README.md): a [sessions](sessions/README.md) middleware before the csrf one (a request without one is the program's mistake, a 500) |
| `double_submit` | a cookie of a random value and its HMAC-SHA256 under the middleware's key; the request sends back the same value. No session needed; a forged cookie fails its signature |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::csrf({.kind = net::http::csrf::tokens::double_submit}));
    srv.route("GET /form", [](net::http::request req, net::http::response_writer w) {
        w.write(net::http::csrf::token(req));
    });
    srv.route("POST /act", [](net::http::request, net::http::response_writer w) { w.write("done"); });
    net::http::response_recorder page;
    page.serve(srv, net::http::test_request("GET", "/form"));
    net::http::cookie page_cookie(page.header("Set-Cookie"));
    string cookie = page_cookie.name + "=" + page_cookie.value;
    for (string sent : {page.body(), string("guessed")}) {
        auto post = net::http::test_request("POST", "/act");
        post.set_header("Cookie", cookie);
        post.set_header("X-CSRF-Token", sent);
        net::http::response_recorder rec;
        rec.serve(srv, post);
        println("{}", rec.status());
    }
}
```

Output:

```text
200
403
```

## See also

- [csrf::options](csrf-options.md)
- [csrf](csrf/README.md)
