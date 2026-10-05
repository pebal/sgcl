[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::close_client_connections

```cpp
void close_client_connections() const;
```

Closes every connection the clients have open with the server now, the server going on (Go's
`CloseClientConnections`): a request in progress sees its connection end, and its handler's
[stop](../request/stop.md) is stopped where the server notices (a write that fails; the end of an HTTP/2
connection). A new request makes a new connection. For a test of a client's or a proxy's behaviour when the
connection is lost.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of connections.

## Exceptions

`invalid_argument` for a moved-from server.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto handler = [](net::http::request, net::http::response_writer w) -> async::task<> {
        for (int i : range(100)) {
            w.write(to_string(i) + "\n");
            if (!co_await w.async_flush()) {
                co_return;
            }
            co_await async::sleep(10 * millisecond);
        }
    };
    net::http::test_server ts(handler);
    net::http::response r = ts.client().get(ts.url());
    ts.close_client_connections();
    println("{}", r.text().has_value());  // cut short
    println("{}", ts.client().get(ts.url() + "/x")->status());
}
```

Output:

```text
false
200
```

## See also

- [close, async_close](close.md): the server closed too
- [sgcl::net::http::test_server](README.md)
