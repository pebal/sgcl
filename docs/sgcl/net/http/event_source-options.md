[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [event_source](event_source/README.md) › options

# sgcl::net::http::event_source::options

```cpp
#include "sgcl/net/http/events.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class event_source {
    public:
        struct options {
            http::headers headers;
            duration retry = 3 * second;
            int max_reconnects = -1;
            size_t max_event_bytes = event_reader::default_max_event_bytes;
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::event_source::options` is how an [event_source](event_source/README.md) asks for its stream and
how long it goes on asking. A plain struct, its fields set by name; the constructors without it take its defaults.

## Member objects

| Member | Description |
|---|---|
| `headers` | the request's fields (`Authorization`, `Cookie`); empty by default |
| `retry` | the reconnection time until the stream sets one by `retry`; 3 s by default, as browsers wait |
| `max_reconnects` | how many reconnections in a row may give no event before it gives up (`io::errc::unexpected_eof`); below zero, the default, no end |
| `max_event_bytes` | the most bytes of an event and of a line, past which it gives up with `net::errc::body_too_large`; 1 MB by default |
| `stop` | gives up when stopped (`ECANCELED`), a reconnection's wait included; none by default |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /down", [](net::http::request, net::http::response_writer w) {
        w.error(net::http::status::service_unavailable);
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::event_source::options o;
    o.retry = 10 * millisecond;
    o.max_reconnects = 2;
    o.headers.set("Authorization", "Bearer token");
    net::http::event_source down(base + "/down", o);
    println("{}", down.next().error().code() == net::errc::http_status);  // not 200: no reconnection

    net::http::event_source nowhere("http://127.0.0.1:1/events", o);
    println("{}", nowhere.next().error().code() == io::errc::unexpected_eof);  // two tries, then no more
    srv.close();
}
```

Output:

```text
true
true
```

## See also

- [(constructor)](event_source/event_source.md): what takes them
- [event_source](event_source/README.md)
