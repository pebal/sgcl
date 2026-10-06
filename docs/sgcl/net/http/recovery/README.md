[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::recovery

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class recovery;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A handler's exception caught, a [middleware](../middleware.md): logged through [slog](../../../slog/README.md) at error,
with the method, the path and what was thrown, and answered — a 500 of the server's own fields by default, or what the
program's answer writes — when nothing of the response has gone yet. A response whose head has gone (a handler that
flushed, then threw) is broken off instead: a chunked body without its last chunk, an HTTP/2 stream reset, so that no
client takes what went for a whole response. The server catches a handler's exception by itself too (a 500, and
`on_error`); this middleware adds the record through slog and the answer of the program's, and catches what a
middleware inside it throws as well.

## Rules

- A plain handler's throw is caught where it is called; a task handler's where the task is awaited, the middleware's
  own frame made only then.
- The connection ends after the response, as after any handler's exception.
- A handle of one word: copies share the settings.

## Member types

| Type | Definition |
|---|---|
| [options](../recovery-options.md) | the logger and the answer |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](recovery.md) | the middleware of the options |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::recovery());
    srv.route("GET /divide", [](net::http::request req, net::http::response_writer w) {
        int d = std::stoi(req.query("by").str());
        if (d == 0) {
            throw std::domain_error("division by zero");
        }
        w.write(to_string(100 / d));
    });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/divide?by=0"));
    print("{} {}", rec.status(), rec.body());
}
```

Sample output:

```text
time=2026-10-06T09:41:07.312+02:00 level=ERROR msg="handler threw" method=GET path=/divide error="division by zero"
500 Internal Server Error
```

## See also

- [recovery::options](../recovery-options.md)
- [request_log](../request_log/README.md)
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
