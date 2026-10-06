[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cache](README.md)

# sgcl::net::http::cache::erase

```cpp
void erase(const string& url) const;
```

The entries of a URL dropped, every variant of it: what a program does when it knows the resource changed. The URL is
compared as the client keys it: without its fragment.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the URL |

## Return value

None.

## Complexity

Linear in the variants of the URL.

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
    println("{}", web.get(url)->text().value());
    web.cache->erase(url);
    println("{}", web.get(url)->text().value());
    srv.close();
}
```

Output:

```text
news 1
news 2
```

## See also

- [clear](clear.md)
- [cache](README.md)
