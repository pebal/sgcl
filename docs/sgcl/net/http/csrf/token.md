[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [csrf](README.md)

# sgcl::net::http::csrf::token

```cpp
static string token(const request& req);
```

The token of the request, for its page: a hidden field named as the options' `field` in a form, or a meta tag a script
reads into the `X-CSRF-Token` field. With synchronizer tokens it is the session's, made and kept in it the first time
(the session then saved, its cookie set); with double-submit tokens it is the request's cookie's, or a new one, whose
cookie is set when the head has not gone. 256 random bits, base64url (and a signature after a dot, for double-submit).
`""` without tokens, or for a request no csrf middleware ran for.

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request, in a handler under a csrf middleware |

## Return value

The token, or `""`.

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
    srv.use(net::http::sessions::in_memory());
    srv.use(net::http::csrf({.kind = net::http::csrf::tokens::synchronizer}));
    srv.route("GET /form", [](net::http::request req, net::http::response_writer w) {
        w.write(net::http::csrf::token(req));
    });
    srv.route("POST /act", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto form = co_await req.async_form();
        w.write("ordered " + form->get("item"));
    });
    net::http::response_recorder page;
    page.serve(srv, net::http::test_request("GET", "/form"));
    auto post = net::http::test_request("POST", "/act", "item=book&csrf_token=" + page.body());
    post.set_header("Content-Type", "application/x-www-form-urlencoded");
    net::http::cookie page_cookie(page.header("Set-Cookie"));
    post.set_header("Cookie", page_cookie.name + "=" + page_cookie.value);
    net::http::response_recorder done;
    done.serve(srv, post);
    println("{} {}", page.body().size(), done.body());
}
```

Output:

```text
43 ordered book
```

## See also

- [csrf::tokens](../csrf-tokens.md)
- [csrf](README.md)
