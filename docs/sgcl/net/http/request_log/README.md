[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::request_log

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class request_log;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A record of every exchange through [slog](../../../slog/README.md), a [middleware](../middleware.md), written once the
response has gone: `request` at info, or at error for a 5xx, with `method`, `path`, `proto`, `status`, `bytes` (of the
body), `duration` (from the middleware), `remote`, `user_agent`, and `request_id` when the request has an
`X-Request-ID`. The same record as the server's [access_log](../server/access_log.md), which logs every exchange of the
server; this one is for a route or a group of them, or for a logger of their own.

## Rules

- Nothing managed per record: the attributes are views of the request and the response's counts.
- A handle of one word: copies share the logger.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](request_log.md) | the middleware of a logger |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /api/items", net::http::request_log().wrap([](net::http::request,
                                                                 net::http::response_writer w) {
        w.write("[]");
    }));
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/api/items"));
}
```

Sample output:

```text
time=2026-10-06T09:41:07.312+02:00 level=INFO msg=request method=GET path=/api/items proto=HTTP/1.1 status=200 bytes=2 duration=14.291µs remote=192.0.2.1:1234 user_agent=""
```

## See also

- [server::access_log](../server/access_log.md): the same record of every exchange of a server
- [recovery](../recovery/README.md)
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
