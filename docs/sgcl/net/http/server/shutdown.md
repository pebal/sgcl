[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](../server.md)

# sgcl::net::http::server::shutdown, async_shutdown

```cpp
/*(1)*/ void shutdown() const;
/*(2)*/ async::task<> async_shutdown() const noexcept;
```

Stops the server gracefully, Go's `Server.Shutdown`: closes the listeners and the idle connections, lets each active
one finish its response (which says `Connection: close`), and returns when every connection has ended. Every
[serve](serve.md) of the server then returns [errc](../../errc.md)`::server_closed`, as does a `serve` after it.

An HTTP/2 connection is sent GOAWAY as Go sends it, first with the largest identifier and then, after a PING's
answer, with the last stream seen; the streams already begun finish, and the connection is closed when its last
stream has. An idle one is closed at once.

There is no limit on the wait: a handler that never returns holds it. A limit is
`co_await async::with_timeout(server.async_shutdown(), 10s)` ([with_timeout](../../../async/with_timeout.md)), then
[close](close.md), as Go's context deadline is.

1. Blocks the calling thread, a thread of the program's, never a worker.
2. The same for a task.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of connections, and the time the active ones take to finish.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

A handler still at work when the shutdown comes finishes its response:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::http::server srv;
    srv.route("GET /slow", [](net::http::request, net::http::response_writer w) -> async::task<> {
        co_await async::sleep(200ms);
        w.write("finished\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto url = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/slow";

    net::http::client web;
    auto asked = async::spawn(web.async_get(url));
    async::sleep(50ms).wait();
    srv.shutdown();
    println(serving.wait().error().code() == net::errc::server_closed);

    net::http::response res = asked.wait();
    string text = res.text();
    print("{} {}", res.header("Connection"), text);
}
```

Output:

```text
true
close finished
```

## See also

- [close](close.md): at once
- [sgcl::net::http::server](../server.md)
