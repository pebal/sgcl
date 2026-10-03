[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::method

```cpp
string method() const noexcept;
```

Returns the method, Go's `r.Method`: as the program gave it to the [constructor](request.md), or as it came on the wire
to a server, in its case (methods are case-sensitive).

## Parameters

None.

## Return value

The method.

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
    srv.route("/items", [](net::http::request req, net::http::response_writer w) {
        w.write("asked with " + req.method() + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    for (const char* method : {"GET", "DELETE", "PURGE"}) {
        net::http::request req(method, base + "/items");
        print("{}: {}", req.method(), web.send(req)->text().value());
    }
    srv.close();
}
```

Output:

```text
GET: asked with GET
DELETE: asked with DELETE
PURGE: asked with PURGE
```

## See also

- [route](../server/route.md): a pattern with a method
- [sgcl::net::http::request](README.md)
