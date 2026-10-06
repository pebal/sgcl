[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cache](README.md)

# sgcl::net::http::cache::size

```cpp
size_t size() const;
```

How many responses the cache holds: every variant of every URL.

## Parameters

None.

## Return value

The number of responses.

## Complexity

Linear in the number of URLs.

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
        w.write("news " + to_string(asked.load()));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/news";

    net::http::client web;
    web.cache = net::http::cache();
    (void)web.get(url)->text();
    println("{} {}", web.cache->size(), web.cache->bytes());
    srv.close();
}
```

Output:

```text
1 6
```

## See also

- [bytes](bytes.md)
- [cache](README.md)
