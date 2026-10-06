[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [response](response/README.md) › cache_status

# sgcl::net::http::response::cache_status

```cpp
#include "sgcl/net/http/response.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class response {
    public:
        enum class cache_status : uint8_t {
            none,
            miss,
            hit,
            revalidated,
            stale,
        };
    };
}
```

How the client's [cache](cache/README.md) answered a request, [from_cache](response/from_cache.md)'s value.

| Value | Description |
|---|---|
| `none` | no cache, or a request it does not take (a POST, a request with `Cache-Control: no-store`) |
| `miss` | from the server: stored when it may be, not when it may not |
| `hit` | a fresh stored response, no request sent |
| `revalidated` | the stored response renewed by a 304 of the server |
| `stale` | a stale stored response: under `stale-while-revalidate` (asked again behind it), or `stale-if-error` with the server failing |

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
    srv.route("GET /news", [&asked](net::http::request, net::http::response_writer w) {
        ++asked;
        w.set_header("Cache-Control", "max-age=0, stale-while-revalidate=60");
        w.write("news " + to_string(asked.load()));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/news";

    net::http::client web;
    web.cache = net::http::cache();
    for (int i : range(2)) {
        auto res = web.get(url);
        println("{} {}", res->text().value(), res->from_cache() == net::http::response::cache_status::stale);
    }
    srv.close();
}
```

Output:

```text
news 1 false
news 1 true
```

## See also

- [response::from_cache](response/from_cache.md)
