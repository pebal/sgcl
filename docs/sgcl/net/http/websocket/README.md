[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::websocket

```cpp
#include "sgcl/net/http/websocket.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class websocket {
    public:
        struct options;   // subprotocols, fields, origins, the limit, keep-alive, compression, a stop
        struct message;   // a message received: text or bytes
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::websocket` is a WebSocket connection (RFC 6455): messages of text or of bytes both ways over one
connection, made by a client, [connect](connect.md) (or a client's own [websocket](../client/websocket.md), through its
proxy and TLS settings), or by a server's handler, [accept](accept.md), which answers the request's upgrade and takes
the connection over from the server. Go's standard library has no WebSocket (gorilla/websocket and
`golang.org/x/net/websocket` are outside it); here it is a type of the HTTP module, and permessage-deflate (RFC 7692)
is in it.

A connection is a handle of one word: a copy is the same connection, and a handle passed into a task keeps it. A
message is sent whole, as one frame ([send](send.md)); [receive](receive.md) puts the frames of the next message
together and answers on its way what the peer asks: a ping by its pong, a close by its close.

## Rules

- **Messages**: text (UTF-8, checked as it comes: a byte that cannot be there fails the connection at once with close
  1007) or bytes; at most `options::max_message_bytes` (32 MB), past which the connection fails with close 1009.
- **One receive at a time**, any number of sends from any number of tasks: each frame is one write, and the
  frames of two sends never mix.
- **Masking**: a client masks every frame with a key from `crypto::random`, a server never does, and each side refuses
  a frame masked the other way (close 1002), as it refuses a reserved bit or opcode, a control frame fragmented or
  longer than 125 bytes, a continuation with no message, a message inside another.
- **Closing**: [close](close.md) sends a close frame (1000 by default, a code and a reason of up to 123 bytes),
  waits for the peer's at most 5 s (read by the receive that waits, when there is one) and closes the connection. A
  close of the peer is answered with the same code and the connection closed; every call after it is
  `net::errc::websocket_closed`, the peer's code in [close_status](close_status.md) and its reason in
  [close_reason](close_reason.md). After this side's close every call is `io::errc::closed`.
- **Keep-alive**: with `options::ping_interval`, a ping each interval, and the connection closed when nothing came for
  two (`ETIMEDOUT`); the pongs are read by receive, so a program that keeps a connection alive keeps a receive
  waiting on it. `options::stop` closes the connection with 1001 (going away) when stopped (`ECANCELED`).
- **permessage-deflate** when `options::compression` is on at both ends: the client offers it, the server takes the
  first offer that does not ask it for a window under 32 KB (`server_max_window_bits` below 15). Each direction keeps
  its window from one message to the next unless its `no_context_takeover` was agreed; the compressor is the compress
  module's DEFLATE at level 1, a sync flush per message.
- **Not over HTTP/2** (RFC 8441): a client asks for `http/1.1` alone by ALPN on `wss://`, and [accept](accept.md) of a
  request that came over HTTP/2 answers 505.
- The blocking forms run the exchange on the scheduler and wait for it, so they are for a thread of the program,
  never a handler; a handler awaits the `async_` forms.

## Member types

| Type | Definition |
|---|---|
| [options](../websocket-options.md) | the settings of a connection |
| [message](../websocket-message.md) | a message received: text or bytes |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](websocket.md) | no connection |
| [connect, async_connect](connect.md) | a client's connection to a URL (static) |
| [accept, async_accept](accept.md) | a server's connection of a request's upgrade (static) |
| [send, async_send](send.md) | sends a message of text or of bytes |
| [receive, async_receive](receive.md) | the next message |
| [ping, async_ping](ping.md) | sends a ping |
| [close, async_close](close.md) | the close handshake |

#### Observers

| Function | Description |
|---|---|
| [subprotocol](subprotocol.md) | the subprotocol the server chose |
| [compression](compression.md) | whether permessage-deflate was agreed |
| [close_status](close_status.md) | the code of the peer's close |
| [close_reason](close_reason.md) | the reason of the peer's close |
| [is_closed](is_closed.md) | whether the connection is closed |
| [connection](connection.md) | the connection under it |
| [operator bool](operator_bool.md) | whether there is a connection |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two handles are the same connection |

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
            co_await ws.async_send("echo: " + m->text());
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "ws://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/echo";

    net::http::websocket ws = net::http::websocket::connect(url).value();
    ws.send("hello");
    println("{}", ws.receive()->text());
    ws.close();
    srv.close();
}
```

Output:

```text
echo: hello
```

## See also

- [client::websocket](../client/websocket.md): through a client's proxy and TLS settings
- [server](../server/README.md), [response_writer::hijack](../response_writer/hijack.md): the connection taken over
- [errc](../../errc.md): `websocket_handshake`, `websocket_protocol`, `websocket_closed`
- RFC 6455, RFC 7692; `tests/net/http/websocket.cpp` (the codec, cases in Autobahn's manner written from the RFC, a
  peer written by hand in Go from the RFC with the standard library alone)
