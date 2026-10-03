[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](README.md)

# sgcl::net::http::server::server

```cpp
server() noexcept;
```

Constructs a server without routes, its settings at their defaults ([Member objects](README.md#member-objects)).
The state the copies share — the routes and the connections — is made at once, so that a copy made later is the same
server. A copy has settings of its own: its fields are copied and change apart from the original's. There is no move
of its own: a move copies, so a moved-from server is the same server, its `on_error` and access log kept.

## Parameters

None.

## Complexity

Constant: one managed allocation.

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
    net::http::server copy = srv;
    copy.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.write("a route of the copy\n");
    });
    copy.max_body_bytes = 1024;
    println("{} {}", srv.max_body_bytes, copy.max_body_bytes);

    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    net::http::client web;
    net::http::response res =
        web.get("http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/");
    string text = res.text();
    print(text);
    srv.shutdown();
}
```

Output:

```text
33554432 1024
a route of the copy
```

## See also

- [route](route.md): the routes the copies share
- [sgcl::net::http::server](README.md)
