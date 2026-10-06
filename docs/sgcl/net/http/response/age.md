[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](README.md)

# sgcl::net::http::response::age

```cpp
optional<duration> age() const noexcept;
```

The age of a response the client's [cache](../cache/README.md) served (RFC 9111 §4.2.3): how long ago the server made
it, in whole seconds, also its `Age` field. `nullopt` for a response from the server.

## Parameters

None.

## Return value

The age, or `nullopt`.

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
        w.write("news " + to_string(asked.load()));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/news";

    net::http::client web;
    web.cache = net::http::cache();
    auto first = web.get(url);
    (void)first->text();
    auto second = web.get(url);
    (void)second->text();
    println("{} {}", first->age().has_value(), second->age().has_value());
    srv.close();
}
```

Output:

```text
false true
```

## See also

- [from_cache](from_cache.md)
- [response](README.md)
