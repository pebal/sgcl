[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [websocket](README.md)

# sgcl::net::http::websocket::send, async_send

```cpp
expected<void, io::error> send(const string& text) const;                                             // (1)
expected<void, io::error> send(const slice<const byte>& data) const;                                  // (2)
template<class T> expected<void, io::error> send(const T& text) const;                                // (3)
async::task<expected<void, io::error>> async_send(const string& text) const noexcept;                 // (4)
async::task<expected<void, io::error>> async_send(const slice<const byte>& data) const noexcept;      // (5)
template<class T> async::task<expected<void, io::error>> async_send(const T& text) const noexcept;    // (6)
```

Sends a message, whole, as one frame: masked by a client, compressed when permessage-deflate was agreed. Any number of
tasks may send at once: each frame is one write, and two never mix.

- (1, 4) A text message. The text is sent as it is: it is the program's to be UTF-8, which the peer checks.
- (2, 5) A message of bytes.
- (3, 6) A literal, a character array, a `std::string_view`: a text message, as (1); takes part only for those.
- (1–3) Write on the calling thread, which waits: for a thread of the program, never a handler.
- (4–6) The same for a task; the task holds the text or the bytes until the frame is written.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text of the message |
| `data` | the bytes of the message |

## Return value

Nothing; or the [io::error](../../../io/error/README.md): `io::errc::closed` after this side's close,
`net::errc::websocket_closed` after the peer's, the connection's error (the connection then closed).

## Complexity

Linear in the message: the mask, the compression.

## Exceptions

- (1–3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (4–6) None.

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
    ws.send("text");
    ws.send(vector<byte>{byte(1), byte(2), byte(3)});
    ws.send(string(100000, 'x'));
    for (int i : range(3)) {
        net::http::websocket::message m = ws.receive().value();
        println("{} {}", m.binary ? "bytes" : "text", m.data.size());
    }
    ws.close();
    println("{}", ws.send("late").error().code() == io::errc::closed);
    srv.close();
}
```

Output:

```text
text 4
bytes 3
text 100000
true
```

## See also

- [receive, async_receive](receive.md)
- [sgcl::net::http::websocket](README.md)
