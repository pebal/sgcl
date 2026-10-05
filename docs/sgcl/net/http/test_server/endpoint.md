[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::endpoint

```cpp
net::endpoint endpoint() const noexcept;
```

Returns the address the server listens on, `127.0.0.1` and the port the system chose, for a test that speaks to it
in raw bytes through [tcp::connect](../../tcp/connect.md). An empty endpoint for a moved-from server.

## Parameters

None.

## Return value

The local endpoint of the listener.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) {
        w.write("raw\n");
    });
    net::connection c = net::tcp::connect(ts.endpoint());
    c.write("GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    string answer = c.read_all_text();
    println("{} | {}", answer.view().substr(0, 15), answer.view().substr(answer.size() - 4, 3));
    println("{}", ts.endpoint().address().to_string());
}
```

Output:

```text
HTTP/1.1 200 OK | raw
127.0.0.1
```

## See also

- [url](url.md): the URL of the same address
- [sgcl::net::http::test_server](README.md)
