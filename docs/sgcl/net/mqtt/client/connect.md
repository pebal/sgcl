[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& url);                                                   // (1)
static async::task<expected<client, io::error>> async_connect(string url) noexcept;                              // (2)
static expected<client, io::error> connect(const string& url, const options& o);                                 // (3)
static async::task<expected<client, io::error>> async_connect(string url, options o) noexcept;                   // (4)
static expected<client, io::error> connect(const net::connection& transport, const options& o);                  // (5)
static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept;    // (6)
```

A session with a broker: the connection, CONNECT with what the [options](../client-options.md) say (the client id,
the credentials, clean start and session expiry, keep alive, the will, the limits), CONNACK read and what it says
taken (an assigned client id, the broker's keep alive, its receive maximum, maximum QoS, retain, topic alias
maximum and maximum packet size, which later calls keep).

- (1–4) The broker of the URL: `mqtt://`, `mqtts://`, `ws://`, `wss://`.
- (5–6) Over a connection the program made (a tunnel, a pipe in memory).

## Parameters

| Parameter | Description |
|---|---|
| `url` | the broker's URL |
| `transport` | a connection to it |
| `o` | the options |

## Return value

The session; the dial's error, TLS's, the WebSocket handshake's; `errc::refused` for a CONNACK that refuses
(its reason code: 0x86 bad user name or password, 0x87 not authorized, 0x84 unsupported protocol version, ...);
`net::errc::invalid_url` and `unsupported_scheme` for a URL that is not one of these.

## Complexity

One round trip after the connection.

## Exceptions

- (1), (3), (5) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4), (6) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::broker b;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(b.async_serve(l));
    string url = string::concat("mqtt://", l.local_endpoint().to_string());
    net::mqtt::client::options o;
    o.client_id = "sensor-17";
    o.version = net::mqtt::version::v3_1_1;
    net::mqtt::client c = net::mqtt::client::connect(url, o).value();
    println("{} {}", c.client_id(), c.session_present());
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
sensor-17 false
```

## See also

- [options](../client-options.md)
- [client](README.md)
