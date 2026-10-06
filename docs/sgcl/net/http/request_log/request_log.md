[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request_log](README.md)

# sgcl::net::http::request_log::request_log

```cpp
explicit request_log(const slog::logger& log = slog::default_logger()) noexcept;
```

The middleware of the logger: slog's default logger unless one is given. A logger made with
`slog::options::buffered` writes its records in batches, as the server's access log does by default.

## Parameters

| Parameter | Description |
|---|---|
| `log` | the logger of the records |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    net::http::request_log logged{slog::logger(kept)};
    net::http::server srv;
    srv.route("GET /a", logged.wrap([](net::http::request, net::http::response_writer w) { w.write("a"); }));
    srv.route("GET /b", [](net::http::request, net::http::response_writer w) { w.write("b"); });
    for (const char* path : {"/a", "/b", "/a"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("GET", path));
    }
    println("{} records", kept.size());
}
```

Output:

```text
2 records
```

## See also

- [request_log](README.md)
