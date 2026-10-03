[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](README.md)

# sgcl::net::http::server::close

```cpp
void close() const;
```

Stops the server at once, Go's `Server.Close`: every listener and connection closed, HTTP/2 ones with their streams,
the reads and writes in progress ended with `io::errc::closed`, and the [stop](../request/stop.md) token of every
request stopped, so that a handler at work can tell. It does not wait for the handlers: one that goes on finds its
reads and writes failing. Every [serve](serve.md) of the server then returns [errc](../../errc.md)`::server_closed`,
as does a `serve` after it.

`close` is also what follows a [shutdown](shutdown.md) that took too long.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of connections.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::channel<string> seen;
    net::http::server srv;
    srv.route("GET /long", [&](net::http::request req,
                               net::http::response_writer w) -> async::task<> {
        while (!req.stop().stop_requested()) {
            co_await async::sleep(10ms);
        }
        co_await seen.send("the handler saw the stop");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto url = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/long";

    net::http::client web;
    auto asked = async::spawn(web.async_get(url));
    async::sleep(50ms).wait();
    srv.close();
    println("{}", serving.wait().error().code() == net::errc::server_closed);
    println("{}", seen.receive().wait().value());
    println("the request failed: {}", !asked.wait());
}
```

Output:

```text
true
the handler saw the stop
the request failed: true
```

## See also

- [shutdown](shutdown.md): gracefully
- [request::stop](../request/stop.md): the token a handler watches
- [sgcl::net::http::server](README.md)
