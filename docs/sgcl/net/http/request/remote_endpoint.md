[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::remote_endpoint

```cpp
net::endpoint remote_endpoint() const noexcept;
```

Returns the address and the port of the client a received request came from, Go's `r.RemoteAddr`, as a
[net::endpoint](../../endpoint.md): the peer of the connection, not a field of the request (a proxy in front of the
server puts the client's own address in a field such as `X-Forwarded-For`, read with [header](header.md)). A request
the program built has an empty endpoint.

## Parameters

None.

## Return value

The endpoint of the client.

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
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        net::endpoint from = req.remote_endpoint();
        w.write("from " + from.address().to_string() + ", a port above 0: " +
                (from.port() > 0 ? "true" : "false") + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    print("{}", web.get(base + "/")->text().value());
    srv.close();
}
```

Output:

```text
from 127.0.0.1, a port above 0: true
```

## See also

- [header](header.md): a field of the request
- [sgcl::net::http::request](../request.md)
