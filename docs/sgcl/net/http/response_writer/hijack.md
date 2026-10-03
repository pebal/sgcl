[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::hijack

```cpp
expected<pair<net::connection, io::reader>, io::error> hijack() noexcept;
```

Hands the connection over to the program, Go's `Hijacker.Hijack`, for a WebSocket or a protocol of its own: the
[connection](../../connection/README.md), and a [reader](../../../io/reader/README.md) of what is left of it, whose first bytes are
the ones the server had read past this request. The server then sends nothing more on it, does not close it and
does not log the exchange; what is written, read and closed is the program's.

Only before the head has gone. Over HTTP/2 a response is a stream, not a connection, and there is no hijack (Go has
no Hijacker there either).

## Parameters

None.

## Return value

The connection and the reader of the rest of it. Or the error, the operation `hijack`:

- `io::errc::closed` after the head has gone, or after a hijack;
- `std::errc::operation_not_supported` over HTTP/2.

## Complexity

Constant.

## Exceptions

None.

## Example

A protocol of the program's after a request: the server's handler takes the connection and answers each line upper
case; the client speaks it over a connection of its own.

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /shout", [](net::http::request, net::http::response_writer w) -> async::task<> {
        auto taken = w.hijack();
        if (!taken) {
            co_return;
        }
        auto [c, rest] = *taken;
        co_await c.async_write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: shout\r\n\r\n");
        io::buffered_reader lines(rest);
        while (auto line = co_await lines.async_read_line()) {
            if (!*line) {
                break;
            }
            co_await c.async_write(string(line->value()).to_upper() + "\n");
        }
        co_await c.async_close();
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::connection c = net::tcp::connect(incoming.local_endpoint());
    c.write("GET /shout HTTP/1.1\r\nHost: x\r\nUpgrade: shout\r\n\r\nhello\nworld\n");
    c.close_write();
    print("{}", c.read_all_text().value());
    srv.shutdown();
}
```

Output:

```text
HTTP/1.1 101 Switching Protocols
Upgrade: shout

HELLO
WORLD
```

## See also

- [connection](../../connection/README.md): what is handed over
- [sgcl::net::http::response_writer](README.md)
