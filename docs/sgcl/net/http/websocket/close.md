[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::close, async_close

```cpp
expected<void, io::error> close() const;                                                                   // (1)
expected<void, io::error> close(uint16_t code, const string& reason) const;                                // (2)
async::task<expected<void, io::error>> async_close() const noexcept;                                       // (3)
async::task<expected<void, io::error>> async_close(uint16_t code, const string& reason) const noexcept;    // (4)
```

The close handshake of this side (RFC 6455 §7.1.2): a close frame of the code and the reason, the peer's close
awaited at most 5 s (read here, what comes before it dropped, or by the receive that waits in another task), then
the connection closed. Every call after it is `io::errc::closed`; a second close does nothing.

- (1, 3) The code 1000, normal closure, and no reason.
- (2, 4) `code` and `reason`: a code a frame may carry (1000-1003, 1007-1014, 3000-4999) and a reason of up to 123
  bytes of UTF-8; anything else is `EINVAL`, and nothing is sent.
- (1–2) On the calling thread, which waits: for a thread of the program, never a handler.
- (3–4) The same for a task.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the close code: 1000 normal, 1001 going away, 1008 policy, 3000-4999 the program's |
| `reason` | why, at most 123 bytes of UTF-8 |

## Return value

Nothing; or `EINVAL` for a code or a reason that may not be sent. A peer that does not answer is no error: the
connection is closed after 5 s.

## Complexity

One round trip, the peer's answer.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("/ws", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        net::http::websocket ws = (co_await net::http::websocket::async_accept(req, w)).value();
        co_await ws.async_receive();  // until the client's close
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/ws";

    net::http::websocket ws = net::http::websocket::connect(url).value();
    println("{}", ws.close(999, "no such code").error().code() == std::errc::invalid_argument);
    println("{}", ws.close(4000, "work done").has_value());
    println("{}", ws.is_closed() && ws.close_status() == 4000);  // the server's echo
    srv.close();
}
```

Output:

```text
true
true
true
```

## See also

- [close_status](close_status.md), [close_reason](close_reason.md): the peer's
- [sgcl::net::http::websocket](README.md)
