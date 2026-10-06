[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request_log](README.md)

# sgcl::net::http::request_log::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: what [server::use](../server/use.md) does for every request,
done for the requests of that route. The record is written once the response has gone.

## Parameters

| Parameter | Description |
|---|---|
| `next` | the handler the request goes on to |

## Return value

The handler of the middleware and `next`.

## Complexity

Constant.

## Exceptions

What the copy of a function throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    net::http::server srv;
    srv.route("GET /fail", net::http::request_log(slog::logger(kept)).wrap([](net::http::request,
                                                                             net::http::response_writer w) {
        w.error(503);
    }));
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/fail"));
    for (const slog::record& r : kept.records()) {
        println("{} {}", r.level() == slog::level::error ? "error" : "info", r.message());
    }
}
```

Output:

```text
error request
```

## See also

- [server::use](../server/use.md)
- [request_log](README.md)
