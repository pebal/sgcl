[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::stop

```cpp
async::stop_token stop() const noexcept;
```

Returns a [stop token](../../../async/stop_token.md) of a received request, Go's `r.Context()`: it is stopped when
the server closes ([close](../server/close.md), or the end of a [shutdown](../server/shutdown.md) for this
connection) and when a write of the response fails, the client gone. A long handler waits on it or checks it, and
gives up the work nobody will receive. A request the program built has a token that is never stopped.

## Parameters

None.

## Return value

The stop token of the request.

## Complexity

Constant.

## Exceptions

None.

## Example

A handler that waits for an event that never comes, until the server closes:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::event done;
    net::http::server srv;
    srv.route("GET /wait", [done](net::http::request req, net::http::response_writer w)
                                   -> async::task<> {
        async::stop_token stop = req.stop();
        while (!stop.stop_requested()) {
            co_await async::sleep(10ms);
        }
        done.set();
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    auto asking = async::spawn(net::http::client().async_get(base + "/wait"));
    async::sleep(100ms).wait();
    srv.close();
    done.wait();
    println("the handler saw the stop");
}
```

Output:

```text
the handler saw the stop
```

## See also

- [close](../server/close.md): what stops it
- [sgcl::net::http::request](../request.md)
