[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [rate_limit](README.md)

# sgcl::net::http::rate_limit::keys

```cpp
size_t keys() const;
```

How many buckets the middleware keeps now: one for each key seen and not yet dropped as idle.

## Parameters

None.

## Return value

The count.

## Complexity

Constant, under the buckets' mutex.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::rate_limit limit(10, 10, {.key = [](const net::http::request& req) { return req.query("user"); }});
    net::http::server srv;
    srv.use(limit);
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("ok"); });
    for (const char* user : {"ann", "bob", "ann", "cy"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("GET", string("/?user=") + user));
    }
    println("{}", limit.keys());
}
```

Output:

```text
3
```

## See also

- [rate_limit](README.md)
