[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [body_limit](README.md)

# sgcl::net::http::body_limit::body_limit

```cpp
explicit body_limit(uint64_t max_bytes) noexcept;
```

The middleware of a limit: a body of at most `max_bytes`; 0 takes none.

## Parameters

| Parameter | Description |
|---|---|
| `max_bytes` | the most bytes a body may have |

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
    net::http::body_limit none(0);
    net::http::server srv;
    srv.route("POST /ping", none.wrap([](net::http::request, net::http::response_writer w) { w.write("pong"); }));
    net::http::response_recorder empty, full;
    empty.serve(srv, net::http::test_request("POST", "/ping"));
    full.serve(srv, net::http::test_request("POST", "/ping", "x"));
    println("{} {}", empty.status(), full.status());
}
```

Output:

```text
200 413
```

## See also

- [body_limit](README.md)
