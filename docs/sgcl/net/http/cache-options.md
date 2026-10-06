[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [cache](cache/README.md) › options

# sgcl::net::http::cache::options

```cpp
#include "sgcl/net/http/cache.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class cache {
    public:
        struct options {
            uint64_t max_bytes = uint64_t(64) << 20;
            uint64_t max_entry_bytes = uint64_t(8) << 20;
            double heuristic = 0.1;
            duration heuristic_max = std::chrono::hours(24);
        };
    };
}
```

`sgcl::net::http::cache::options` are the limits of a [cache](cache/README.md): a plain struct, its fields set by name.

## Member objects

| Member | Description |
|---|---|
| `max_bytes` | the bodies kept together; past it the least recently used entries go. 64 MB by default |
| `max_entry_bytes` | a larger body is not stored. 8 MB by default |
| `heuristic` | a response without an explicit freshness (no `max-age`, no `Expires`) is fresh for this fraction of the time since its `Last-Modified` (RFC 9111 §4.2.2). 0.1 by default |
| `heuristic_max` | at most this long. 24 hours by default |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

#include <atomic>

using namespace sgcl;

int main() {
    std::atomic<int> asked{0};
    net::http::server srv;
    srv.route("GET /report", [&asked](net::http::request, net::http::response_writer w) {
        ++asked;
        w.set_header("Last-Modified", "Mon, 01 Jan 2024 00:00:00 GMT");
        w.write("report");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/report";

    net::http::cache::options none;
    none.heuristic = 0;  // no freshness without max-age or Expires
    net::http::client web;
    web.cache = net::http::cache(none);
    (void)web.get(url)->text();
    (void)web.get(url)->text();
    println("{}", asked.load());
    srv.close();
}
```

Output:

```text
2
```

## See also

- [cache](cache/README.md)
