[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [rate_limit](rate_limit/README.md) › options

# sgcl::net::http::rate_limit::options

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class rate_limit {
    public:
        struct options {
            string header;
            function<string(const request&)> key;
            duration idle = std::chrono::minutes(10);
            size_t max_keys = 100000;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::rate_limit::options` is how a [rate_limit](rate_limit/README.md) keys its buckets and how many it
keeps: a plain struct, its fields set by name.

## Member objects

| Member | Description |
|---|---|
| `header` | the key is the value of this field (`"X-API-Key"`); a request without it is the key `""`. Empty, the default: the client's address |
| `key` | the key a function of the program's makes of the request; it wins over `header` |
| `idle` | a bucket dropped after this long without a request of its key; 10 minutes by default |
| `max_keys` | the most buckets: past it the idle ones go first, then the least recently used; 100 000 by default |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::rate_limit::options o;
    o.key = [](const net::http::request& req) { return req.header("X-Tenant"); };
    o.max_keys = 2;
    net::http::rate_limit limit(5, 5, o);
    net::http::server srv;
    srv.use(limit);
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("ok"); });
    for (const char* tenant : {"t1", "t2", "t3"}) {
        auto req = net::http::test_request("GET", "/");
        req.set_header("X-Tenant", tenant);
        net::http::response_recorder rec;
        rec.serve(srv, req);
    }
    println("{}", limit.keys());
}
```

Output:

```text
2
```

## See also

- [rate_limit](rate_limit/README.md)
- [sgcl::net::http](README.md)
