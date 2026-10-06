[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [recovery](README.md)

# sgcl::net::http::recovery::recovery

```cpp
recovery();                             // (1)
explicit recovery(const options& o);    // (2)
```

1. The records to slog's default logger, a 500 `text/plain` for an answer.
2. The logger and the answer of the options ([recovery::options](../recovery-options.md)).

## Parameters

| Parameter | Description |
|---|---|
| `o` | the logger and the answer |

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
    net::http::server srv;
    srv.use(net::http::recovery({.log = slog::logger(kept)}));
    srv.route("GET /x", [](net::http::request, net::http::response_writer) { throw std::runtime_error("broken"); });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/x"));
    for (const slog::record& r : kept.records()) {
        println("{}", r.message());
    }
    println("{}", rec.status());
}
```

Output:

```text
handler threw
500
```

## See also

- [recovery](README.md)
