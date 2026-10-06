[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cache](README.md)

# sgcl::net::http::cache::clear

```cpp
void clear() const;
```

Every entry dropped, and in a directory their files removed: the next request of each URL goes to the server.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

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
    web.cache->clear();
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

- [erase](erase.md)
- [cache](README.md)
