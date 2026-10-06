[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::destroy

```cpp
void destroy() noexcept;
```

The session ended: its values gone, its cookie expired in the browser (`Max-Age=0`), an
[in_memory](../sessions/in_memory.md) store's entry dropped at once. A cookie of [in_cookie](../sessions/in_cookie.md)
kept elsewhere stays valid to its time: a store that must end sessions for good is the server's.

## Parameters

None.

## Return value

None.

## Complexity

Constant; the store does the rest when the session is saved.

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
    srv.route("POST /in", [](net::http::request req, net::http::response_writer w) {
        net::http::session(req).set("user", "ann");
    });
    srv.route("POST /out", [](net::http::request req, net::http::response_writer w) {
        net::http::session(req).destroy();
    });
    net::http::response_recorder in;
    in.serve(srv, net::http::test_request("POST", "/in"));
    auto bye = net::http::test_request("POST", "/out");
    net::http::cookie in_cookie(in.header("Set-Cookie"));
    bye.set_header("Cookie", in_cookie.name + "=" + in_cookie.value);
    net::http::response_recorder out;
    out.serve(srv, bye);
    println("{}", out.header("Set-Cookie").contains("Max-Age=0"));
}
```

Output:

```text
true
```

## See also

- [renew](renew.md)
- [session](README.md)
