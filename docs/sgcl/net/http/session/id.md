[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::id

```cpp
string id() const noexcept;
```

The id of the session in an [in_memory](../sessions/in_memory.md) store: 128 random bits, base64url, the value of its
cookie; `""` for a session not yet saved (a new one gets its id as it is saved), and for
[in_cookie](../sessions/in_cookie.md), which has none.

## Parameters

None.

## Return value

The id, or `""`.

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
        w.write(s.id().empty() ? string("none yet") : s.id());
        s.set("a", "1");
    });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    println("{}", rec.body());
}
```

Output:

```text
none yet
```

## See also

- [session](README.md)
