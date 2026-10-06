[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [sessions](sessions/README.md) › options

# sgcl::net::http::sessions::options

```cpp
#include "sgcl/net/http/session.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class sessions {
    public:
        struct options {
            string cookie = "__Host-session";
            duration max_age = std::chrono::hours(24);
            duration idle_timeout = duration::zero();
            string path = "/";
            string domain;
            bool secure = true;
            bool http_only = true;
            string same_site = "Lax";
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::sessions::options` is the cookie of a [sessions](sessions/README.md) store and the lifetime of its
sessions: a plain struct, its fields set by name. The defaults are the secure ones RFC 6265bis and OWASP advise; a
`__Host-` name (the default) is set by a browser only from a secure origin, for the path `/`, without a domain, so no
other site of the domain can set or shadow it.

## Member objects

| Member | Description |
|---|---|
| `cookie` | the cookie's name, a token; `"__Host-session"` by default. A `__Host-` name needs `secure`, `path` `"/"` and no `domain`; a `__Secure-` one needs `secure` |
| `max_age` | the session's lifetime from its start, and the cookie's `Max-Age` (what is left of it); 24 hours by default; zero: the browser's session, no `Max-Age` |
| `idle_timeout` | the longest time between two requests of a session; zero, the default: none |
| `path` | the cookie's `Path`; `"/"` by default |
| `domain` | the cookie's `Domain`; empty, the default: the host alone |
| `secure` | `Secure`: sent over https alone (browsers allow it on `localhost`); `true` by default |
| `http_only` | `HttpOnly`: no script reads the cookie; `true` by default |
| `same_site` | `SameSite`: `"Lax"` (the default: the cookie goes with a top-level navigation from another site, not with its POST), `"Strict"`, `"None"` (needs `secure`), or `""` for none |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::sessions::options o;
    o.cookie = "sid";
    o.max_age = std::chrono::hours(1);
    o.same_site = "Strict";
    net::http::server srv;
    srv.use(net::http::sessions::in_memory(o));
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        net::http::session(req).set("a", "1");
    });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    bool first = true;
    for (auto attribute : rec.header("Set-Cookie").split("; ")) {
        if (!first) {
            println("{}", attribute);
        }
        first = false;
    }
}
```

Output:

```text
Path=/
Max-Age=3600
HttpOnly
Secure
SameSite=Strict
```

## See also

- [sessions](sessions/README.md)
- [cookie](cookie/README.md)
- [sgcl::net::http](README.md)
