[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::is_new

```cpp
bool is_new() const noexcept;
```

Whether the request came without a session: no cookie, one that does not open or names no session of the store, or a
session past its time.

## Parameters

None.

## Return value

`true` for a new session.

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
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        net::http::session s(req);
        w.write(s.is_new() ? "first visit" : "welcome back");
        s.set("seen", "1");
    });
    net::http::response_recorder first;
    first.serve(srv, net::http::test_request("GET", "/"));
    auto again = net::http::test_request("GET", "/");
    net::http::cookie first_cookie(first.header("Set-Cookie"));
    again.set_header("Cookie", first_cookie.name + "=" + first_cookie.value);
    net::http::response_recorder second;
    second.serve(srv, again);
    println("{}, {}", first.body(), second.body());
}
```

Output:

```text
first visit, welcome back
```

## See also

- [session](README.md)
