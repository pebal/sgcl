[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_source](README.md)

# sgcl::net::http::event_source::close

```cpp
void close() const noexcept;
```

Closes the stream's connection and stops the reconnections: every next after it gives `io::errc::closed`. A copy of
the source is the same source, closed with it.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /events", [](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        for (int i : range(3)) {
            net::http::event e;
            e.type = "tick";
            e.id = to_string(i);
            e.data = "event " + to_string(i);
            co_await events.async_send(e);
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/events";
    net::http::client web;
    web.proxy = net::http::proxy();

    net::http::event_source source(web, url, net::http::event_source::options());
    println("{}", source.next()->data);
    source.close();
    println("{}", source.next().error().code() == io::errc::closed);
    srv.close();
}
```

Output:

```text
event 0
true
```

## See also

- [next, async_next](next.md)
- [sgcl::net::http::event_source](README.md)
