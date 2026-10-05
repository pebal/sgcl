[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::server

```cpp
http::server server() const;
```

Returns the [server](../server/README.md) serving, a copy of its handle: its routes are the test server's, so a route
added through it is served at once. Its settings were read when the test server started; changed on the copy, they
do not reach the serving.

## Parameters

None.

## Return value

The server.

## Complexity

Constant.

## Exceptions

`invalid_argument` for a moved-from server.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) {
        w.write("default\n");
    });
    ts.server().route("/late", [](net::http::request, net::http::response_writer w) {
        w.write("added while serving\n");
    });
    print("{}", ts.client().get(ts.url() + "/late")->text().value());
    print("{}", ts.client().get(ts.url() + "/other")->text().value());
}
```

Output:

```text
added while serving
default
```

## See also

- [server::route](../server/route.md)
- [sgcl::net::http::test_server](README.md)
