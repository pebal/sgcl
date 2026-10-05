[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [reverse_proxy](README.md)

# sgcl::net::http::reverse_proxy::operator()

```cpp
async::task<> operator()(request r, response_writer w) const noexcept;
```

Passes the request on and its response back: what a [server](../server/README.md) calls with each request routed
to the proxy (`server.route("/", proxy)`, or a pattern of its own). The backend is chosen, the outgoing request made
(the URL joined, the fields less the hop-by-hop ones, the forwarding fields, the body as a stream), the `rewrite`
hook run, the request sent; a send that fails is tried again on another backend when the request may go again, and
answered 502 (504 for a timeout) when it may not. The response's head is passed back through the
`modify_response` hook, its body as it comes, its trailers after it. A request that asks for an upgrade is sent on
a connection of its own, and on a 101 the two connections become one pipe of bytes until either ends.

The handler is a task: it waits for the backend. A [response_recorder](../response_recorder/README.md)'s writer
takes it as a server's does.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the request received |
| `w` | its writer |

## Return value

The task of the exchange, done when the response has been passed back.

## Complexity

Linear in the fields and in the body.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) {
        w.write("for " + r.header("X-Forwarded-Host") + ", via " + r.header("X-Forwarded-Proto")
                + "\n");
    });
    net::http::reverse_proxy proxy(backend.url());
    // called by hand, as a server calls it
    net::http::response_recorder rec;
    proxy(net::http::test_request("GET", "/"), rec.writer()).wait();
    print("{} {}", rec.status(), rec.body());
}
```

Output:

```text
200 for example.com, via http
```

## See also

- [server::route](../server/route.md): where it is handed to a server
- [sgcl::net::http::reverse_proxy](README.md)
