[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cache](README.md)

# sgcl::net::http::cache::cache

```cpp
cache();                             // (1)
explicit cache(const options& o);    // (2)
```

A cache in memory, empty.

1. With the default limits: 64 MB of bodies, 8 MB at most a body.
2. With the [options](../cache-options.md) given.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the limits and the heuristic |

## Complexity

Constant.

## Exceptions

None.

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
        w.set_header("Cache-Control", "max-age=60");
        w.write(string(std::string(2000, 'n')));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/news";

    net::http::cache::options small;
    small.max_entry_bytes = 1000;  // a body of 2000 bytes is not kept
    net::http::client web;
    web.cache = net::http::cache(small);
    (void)web.get(url)->text();
    (void)web.get(url)->text();
    println("{} {}", asked.load(), web.cache->size());
    srv.close();
}
```

Output:

```text
2 0
```

## See also

- [on_disk](on_disk.md)
- [options](../cache-options.md)
- [cache](README.md)
