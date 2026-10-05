[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::receive, async_receive

```cpp
expected<message, io::error> receive() const;                                // (1)
async::task<expected<message, io::error>> async_receive() const noexcept;    // (2)
```

The next [message](../websocket-message.md): its frames read and put together, a text message checked to be UTF-8
as it comes (a byte that cannot be there fails the connection at once), a compressed one decompressed. On its way it
answers what the peer asks: a ping by its pong, a close by its close, after which it gives
`net::errc::websocket_closed` and the connection is closed. One receive at a time.

A frame that breaks RFC 6455 fails the connection (§7.1.7): a close frame of the code that says why (1002 for the
protocol, 1007 for text that is not UTF-8 or data that is not DEFLATE, 1009 for a message past
`options::max_message_bytes`), the connection closed, the error returned.

1. Reads on the calling thread, which waits: for a thread of the program, never a handler.
2. The same for a task: `while (auto m = co_await ws.async_receive())`.

## Parameters

None.

## Return value

The message; or the [io::error](../../../io/error/README.md): `net::errc::websocket_closed` for the peer's close (its
code in [close_status](close_status.md)), `net::errc::websocket_protocol` for a frame that breaks the protocol (what
in the path), `net::errc::body_too_large` past the limit, `io::errc::closed` after this side's close, `ETIMEDOUT`
when keep-alive heard nothing, `ECANCELED` for the stop, `io::errc::unexpected_eof` for a connection that ended
without a close.

## Complexity

Linear in the message.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("/echo", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        net::http::websocket ws = (co_await net::http::websocket::async_accept(req, w)).value();
        while (auto m = co_await ws.async_receive()) {
            m->binary ? co_await ws.async_send(m->data) : co_await ws.async_send(m->text());
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/echo";

    net::http::websocket ws = net::http::websocket::connect(url).value();
    ws.send("one");
    ws.send("two");
    println("{} {}", ws.receive()->text(), ws.receive()->text());

    net::http::websocket::options small;
    small.max_message_bytes = 8;
    net::http::websocket limited = net::http::websocket::connect(url, small).value();
    limited.send("more than eight bytes");
    auto big = limited.receive();
    println("{}", big.error().code() == net::errc::body_too_large);
    ws.close();
    srv.close();
}
```

Output:

```text
one two
true
```

## See also

- [send, async_send](send.md)
- [message](../websocket-message.md)
- [sgcl::net::http::websocket](README.md)
