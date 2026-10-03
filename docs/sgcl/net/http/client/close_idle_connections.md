[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](../client.md)

# sgcl::net::http::client::close_idle_connections

```cpp
void close_idle_connections() const noexcept;
```

Closes the idle connections of the pool now, Go's `Transport.CloseIdleConnections`: every HTTP/1.1 connection that
waits in the pool, and every HTTP/2 connection that carries no stream, closed with GOAWAY. A connection that carries
a request or a body not read yet is left to it. The pool is the one the copies of the client share; the next request
dials a new connection.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of connections of the pool.

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
        w.write(to_string(req.remote_endpoint().port()));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    string first = web.get(base + "/")->text().value();
    string again = web.get(base + "/")->text().value();
    web.close_idle_connections();
    string fresh = web.get(base + "/")->text().value();
    println("the same connection: {}, after the close: {}", first == again, first == fresh);
    srv.close();
}
```

Output:

```text
the same connection: true, after the close: false
```

## See also

- [idle_timeout](../client.md#member-objects): when an idle connection is closed by itself
- [sgcl::net::http::client](../client.md)
