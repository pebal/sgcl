[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [client](README.md)

# sgcl::net::amqp::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& url);                                                               // (1)
static async::task<expected<client, io::error>> async_connect(string url) noexcept;                                          // (2)
static expected<client, io::error> connect(const string& url, const options& o);                                             // (3)
static async::task<expected<client, io::error>> async_connect(string url, options o) noexcept;                               // (4)
static expected<client, io::error> connect(const net::connection& transport, const string& url, const options& o);           // (5)
static async::task<expected<client, io::error>> async_connect(net::connection transport, string url, options o) noexcept;    // (6)
```

A connection to the broker: the dial, TLS for `amqps://`, the protocol header, the login by PLAIN, the tuning of
channels, frame size and heartbeat (the lower of both sides' values), the vhost opened.

- (1–4) The broker of the URL: `amqp://user:password@host[:5672]/vhost` or `amqps://...[:5671]`; guest:guest and
  "/" when it has none (the vhost percent-encoded: `%2F` is "/").
- (5–6) Over a connection the program made (a tunnel, a unix socket); the URL gives the credentials and the vhost.

The options' timeout bounds the dial, TLS and the handshake together; their stop ends the dial with `ECANCELED`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | where the broker is, the credentials, the vhost |
| `transport` | a connection to it |
| `o` | TLS, heartbeats, frame size, channels, the timeout, the stop |


## Return value

The connection; the dial's error, TLS's, `errc::access_refused` for credentials refused, `errc::not_allowed` for
a vhost the broker does not have, `errc::not_implemented` for a broker of another version of AMQP,
`net::errc::invalid_url` or `net::errc::unsupported_scheme` for a URL that is not one.

## Complexity

A few round trips.

## Exceptions

- (1), (3), (5) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4), (6) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    auto c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/");
    println("{}", bool(c));
    auto refused = net::amqp::client::connect("amqp://guest:wrong@localhost:5672/");
    println("{}", refused.error().code() == net::amqp::errc::access_refused);
}
```

Output:

```text
true
true
```

## See also

- [options](../client-options.md)
- [open_channel](open_channel.md)
- [client](README.md)
