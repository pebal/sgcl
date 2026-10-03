[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](README.md)

# sgcl::net::http::server::not_found

```cpp
template<class Handler>
server& not_found(Handler handler);
```

Registers the handler of a request no pattern matches, Go's handler of a mux's `"/"` that writes its own 404. Without
one, such a request gets the server's 404: `Not Found` as `text/plain`, as
[response_writer::error](../response_writer/error.md) writes it. A path some pattern matches for another method is
still 405 with `Allow`, and the redirects of [route](route.md) (an empty segment, a subtree without its slash) still
go first. Registered before [serve](serve.md); a second call replaces the first.

## Parameters

| Parameter | Description |
|---|---|
| `handler` | a function of `(request, response_writer)` that returns `void` or `async::task<>`; another return type does not compile |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

What the move of `handler` throws; the handler before it is kept then.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.write("hello\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());
    net::http::client web;

    net::http::response plain = web.get(base + "/nothing");
    string text = plain.text();
    print("{} {}", plain.status(), text);
    srv.shutdown();

    net::http::server own;
    own.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.write("hello\n");
    });
    own.not_found([](net::http::request req, net::http::response_writer w) {
        w.set_status(net::http::status::not_found);
        w.write("no page at " + req.url().path() + "\n");
    });
    net::listener again = net::tcp::listen("127.0.0.1:0");
    auto serving_own = async::spawn(own.async_serve(again));
    net::http::response answered =
        web.get("http://127.0.0.1:" + to_string(again.local_endpoint().port()) + "/nothing");
    string text_own = answered.text();
    print("{} {}", answered.status(), text_own);
    own.shutdown();
}
```

Output:

```text
404 Not Found
404 no page at /nothing
```

## See also

- [route](route.md): the patterns
- [sgcl::net::http::server](README.md)
