[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [sessions](README.md)

# sgcl::net::http::sessions::in_memory

```cpp
static sessions in_memory();                    // (1)
static sessions in_memory(const options& o);    // (2)
```

Sessions whose values stay on the server, in memory, under a random id of 128 bits (base64url, 22 characters) that the
cookie carries: a session can be ended at once ([destroy](../session/destroy.md) drops it from the store), and its values
can be larger than a cookie's; it ends with the process, and every process has its own. A session past its lifetime is
dropped when it is met, and the store is swept as it grows. [renew](../session/renew.md) moves the values to a new id
and drops the old one.

1. The default options.
2. The options given.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the cookie and the lifetime ([sessions::options](../sessions-options.md)) |

## Return value

The middleware.

## Complexity

Constant. Each request then finds its id in a hash map under a mutex, and copies the values.

## Exceptions

`invalid_argument` for options a browser would refuse (a `__Host-` cookie that is not secure, has a domain or a path
other than `/`; `SameSite=None` without `Secure`; a name that is not a token).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto store = net::http::sessions::in_memory();
    net::http::server srv;
    srv.use(store);
    srv.route("POST /cart", [](net::http::request req, net::http::response_writer w) {
        net::http::session(req).set("item", req.query("add"));
        w.write("added");
    });
    srv.route("POST /logout", [](net::http::request req, net::http::response_writer w) {
        net::http::session(req).destroy();
        w.write("bye");
    });
    net::http::response_recorder add;
    add.serve(srv, net::http::test_request("POST", "/cart?add=book"));
    println("{} sessions", store.size());
    auto out = net::http::test_request("POST", "/logout");
    net::http::cookie add_cookie(add.header("Set-Cookie"));
    out.set_header("Cookie", add_cookie.name + "=" + add_cookie.value);
    net::http::response_recorder bye;
    bye.serve(srv, out);
    println("{} sessions", store.size());
}
```

Output:

```text
1 sessions
0 sessions
```

## See also

- [in_cookie](in_cookie.md)
- [size](size.md)
- [sessions](README.md)
