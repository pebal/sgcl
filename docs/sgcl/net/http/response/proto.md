[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](README.md)

# sgcl::net::http::response::proto

```cpp
string proto() const noexcept;
```

Returns the protocol the response came over, Go's `resp.Proto`: `"HTTP/2.0"` when the server chose `h2` by ALPN or
the client sent it by prior knowledge (`h2c`), else the version of the status line, `"HTTP/1.1"` or `"HTTP/1.0"`.
Nothing else of the program changes with it.

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
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.write("hello\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client plain;
    net::http::client multiplexed;
    multiplexed.h2c = true;
    for (net::http::client web : {plain, multiplexed}) {
        net::http::response res = web.get(base + "/");
        print("{} {}", res.proto(), res.text().value());
    }
    srv.close();
}
```

Output:

```text
HTTP/1.1 hello
HTTP/2.0 hello
```

## See also

- [request::proto](../request/proto.md): the protocol of a request
- [sgcl::net::http::response](README.md)
