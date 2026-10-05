[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::close, async_close

```cpp
void close();                   // (1)
async::task<> async_close();    // (2)
```

Closes the server and waits for it to end (Go's `Close`): the listener closed, every connection closed, every
request's [stop](../request/stop.md) stopped, the client's idle connections closed, then the handlers awaited to
their ends. The destructor does the same without the wait. A second close, or the close of a moved-from server,
does nothing.

1. Blocks the calling thread until the handlers have ended: for a thread of the program, never a worker.
2. The same for a task: `co_await ts.async_close()`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of connections, and the wait for the handlers.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto handler = [](net::http::request r, net::http::response_writer w) -> async::task<> {
        co_await r.stop().stopped();  // until the server closes
        println("the handler saw the close");
    };
    net::http::test_server ts(handler);
    auto pending = async::spawn(ts.client().async_get(ts.url()));
    async::sleep(50 * millisecond).wait();
    ts.close();
    println("closed: {}", pending.wait().has_value());
    ts.close();  // nothing more
}
```

Output:

```text
the handler saw the close
closed: false
```

## See also

- [close_client_connections](close_client_connections.md): the connections alone
- [sgcl::net::http::test_server](README.md)
