[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::url

```cpp
net::url url() const noexcept;
```

Returns the URL the response came from, Go's `resp.Request.URL`: the last of the redirects the client followed, the
URL of the request when there was none.

## Parameters

None.

## Return value

The URL of the response.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /old", [](net::http::request, net::http::response_writer w) {
        w.redirect("/older", net::http::status::moved_permanently);
    });
    srv.route("GET /older", [](net::http::request, net::http::response_writer w) {
        w.redirect("/new?from=older");
    });
    srv.route("GET /new", [](net::http::request, net::http::response_writer w) {
        w.write("moved here\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/old");
    println("{} {}?{}", res.status(), res.url().path(), res.url().query());
    res.close();
    srv.close();
}
```

Output:

```text
200 /new?from=older
```

## See also

- [max_redirects](../client.md#member-objects): how many redirects are followed
- [request::url](../request/url.md): the URL of a request
- [sgcl::net::http::response](../response.md)
