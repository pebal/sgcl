[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::session

```cpp
#include "sgcl/net/http/session.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class session;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

The session of one request, as a [sessions](../sessions/README.md) middleware loaded it before the handler: string values
by name, kept between the requests of one client. `net::http::session s(req)` in a handler; what it sets is saved as the
response's head is made — at the first flush, or when the handler is done — into the cookie or into the server's store.
[renew](renew.md) gives the session a new identity after a login, [destroy](destroy.md) ends it.

## Rules

- A handle of one word over the request's own state: copies are the same session.
- One request's session is its handler's: not to be changed by two tasks at once. Two requests of one client at once
  each have their own copy, and the last one saved wins, as in most stores.
- What is set after the head has gone (a flush) is not saved.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](session.md) | the session of a request |

#### Values

| Function | Description |
|---|---|
| [get](get.md) | the value of a key |
| [contains](contains.md) | whether there is a value of a key |
| [set](set.md) | a value of a key set |
| [erase](erase.md) | the value of a key gone |
| [clear](clear.md) | every value gone |

#### The session

| Function | Description |
|---|---|
| [id](id.md) | the server store's id |
| [is_new](is_new.md) | whether the request came without a session |
| [renew](renew.md) | a new identity for the same values |
| [destroy](destroy.md) | the session ended |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::sessions::in_memory());
    srv.route("GET /theme", [](net::http::request req, net::http::response_writer w) {
        net::http::session s(req);
        if (!req.query("set").empty()) {
            s.set("theme", req.query("set"));
        }
        w.write(s.contains("theme") ? s.get("theme") : string("light"));
    });
    net::http::response_recorder first;
    first.serve(srv, net::http::test_request("GET", "/theme?set=dark"));
    auto later = net::http::test_request("GET", "/theme");
    net::http::cookie first_cookie(first.header("Set-Cookie"));
    later.set_header("Cookie", first_cookie.name + "=" + first_cookie.value);
    net::http::response_recorder second;
    second.serve(srv, later);
    println("{} {}", first.body(), second.body());
}
```

Output:

```text
dark dark
```

## See also

- [sessions](../sessions/README.md)
- [sgcl::net::http](../README.md)
