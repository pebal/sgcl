[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](README.md)

# sgcl::net::http::response::from_cache

```cpp
cache_status from_cache() const noexcept;
```

How the client's [cache](../cache/README.md) answered the request ([cache_status](../response-cache_status.md)): `none`
without a cache or for a request it does not take, `miss` when the response came from the server, `hit` when a
fresh stored one was served with no request sent, `revalidated` when a 304 renewed the stored one, `stale` when a stale
one was served (`stale-while-revalidate`, or `stale-if-error` when the server failed).

## Parameters

None.

## Return value

How the response came.

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

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /news", [](net::http::request req, net::http::response_writer w) {
        w.set_header("Cache-Control", "no-cache");  // stored, but asked again each time
        w.set_header("ETag", "\"v1\"");
        if (req.header("If-None-Match") == "\"v1\"") {
            w.set_status(304);
            return;
        }
        w.write("news");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/news";

    net::http::client web;
    web.cache = net::http::cache();
    for (int i : range(2)) {
        auto res = web.get(url);
        (void)res->text();
        println("{}", res->from_cache() == net::http::response::cache_status::revalidated);
    }
    srv.close();
}
```

Output:

```text
false
true
```

## See also

- [age](age.md)
- [cache](../cache/README.md)
- [response](README.md)
