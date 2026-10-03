[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::proto

```cpp
string proto() const noexcept;
```

Returns the protocol the request came over, Go's `r.Proto`: `"HTTP/2.0"`, `"HTTP/1.1"` or `"HTTP/1.0"`. On the server
it is the protocol of the connection the request was read from: HTTP/2 chosen by ALPN or by prior knowledge
(`h2c`), else the version of the request line. A request the program builds says `"HTTP/1.1"` until it is sent over
HTTP/2.

## Parameters

None.

## Return value

`"HTTP/2.0"`, `"HTTP/1.1"` or `"HTTP/1.0"`.

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
    srv.h2c = true;
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        w.write("asked over " + req.proto() + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client plain;
    print("{}", plain.get(base + "/")->text().value());
    net::http::client multiplexed;
    multiplexed.h2c = true;
    print("{}", multiplexed.get(base + "/")->text().value());
    srv.close();
}
```

Output:

```text
asked over HTTP/1.1
asked over HTTP/2.0
```

## See also

- [response::proto](../response/proto.md): the protocol of a response
- [sgcl::net::http::request](../request.md)
