[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::csrf

```cpp
#include "sgcl/net/http/csrf.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class csrf;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Cross-site request forgery refused, a [middleware](../middleware.md): a page of another site that makes a browser send
an unsafe request (a form's POST) with the user's cookies does not get it served. The check is Go's
`http.CrossOriginProtection` (Go 1.25), held against it case by case: GET, HEAD and OPTIONS always pass (they must
change nothing); another method passes when `Sec-Fetch-Site`, which every browser sends since 2023, is `same-origin` or
`none` (typed or bookmarked), or when its `Origin` is one of the trusted; without `Sec-Fetch-Site`, a request without
`Origin` passes (not a browser), and one whose `Origin` is trusted or names the request's `Host` passes. Everything else
is 403.

With tokens ([csrf::tokens](../csrf-tokens.md)), an unsafe request must also send back the token [token](token.md) gave
its page — in a field (`X-CSRF-Token`) or a form's field (`csrf_token`) — as OWASP's cheat sheet describes: the
synchronizer token, kept in the request's [session](../session/README.md), or the signed double-submit cookie, a random
value and its HMAC-SHA256 in a cookie the request must echo. Tokens are for clients that send neither `Sec-Fetch-Site`
nor `Origin`; the origin check runs in every mode.

## Rules

- `SameSite=Lax` cookies (the [sessions](../sessions/README.md) default) already keep a cross-site POST from carrying the
  session in every browser since 2020; the origin check covers what they do not (a sibling subdomain is the same site),
  and the tokens cover old clients.
- A token sent in a urlencoded form's field makes the middleware read the body for it; the handler's
  [form](../request/form.md) gives the fields after. A multipart form sends the token in the field.
- Tokens are compared in constant time.
- A handle of one word: copies share the settings and the key.

## Member types

| Type | Definition |
|---|---|
| [options](../csrf-options.md) | the kind of tokens, the trusted origins, the field and cookie names, the refusal |
| [tokens](../csrf-tokens.md) | none, synchronizer, double-submit |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](csrf.md) | the middleware of the options, a key of its own or the one given |
| [token](token.md) | the token of a request, for its page |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::csrf());
    srv.route("POST /transfer", [](net::http::request, net::http::response_writer w) { w.write("sent\n"); });
    for (const char* site : {"same-origin", "cross-site"}) {
        auto req = net::http::test_request("POST", "/transfer");
        req.set_header("Sec-Fetch-Site", site);
        net::http::response_recorder rec;
        rec.serve(srv, req);
        print("{}: {} {}", site, rec.status(), rec.body());
    }
}
```

Output:

```text
same-origin: 200 sent
cross-site: 403 Forbidden
```

## See also

- [csrf::options](../csrf-options.md)
- [cors](../cors/README.md): reading the answers of another origin, allowed
- [sessions](../sessions/README.md)
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
