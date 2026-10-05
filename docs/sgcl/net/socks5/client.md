[sgcl](../../README.md) › [net](../README.md) › [socks5](README.md)

# sgcl::net::socks5::client, async_client

```cpp
static expected<net::connection, io::error> client(const net::connection& transport,                        // (1)
                                                   const string& target);
static expected<net::connection, io::error> client(const net::connection& transport,                        // (2)
                                                   const string& target, const options& o);
static async::task<expected<net::connection, io::error>> async_client(net::connection transport,            // (3)
                                                                     string target) noexcept;
static async::task<expected<net::connection, io::error>> async_client(net::connection transport,            // (4)
                                                                     string target, options o) noexcept;
```

The SOCKS5 handshake over a connection to a proxy there is: the proxy at the other end of `transport` asked to
CONNECT to `target`, and `transport` returned once it has, carrying the target's bytes from then on. It is for a
proxy reached some other way than a TCP connection of its own: over TLS, through another proxy, a pair in memory.
[connect](connect.md) is the dial and this together.

- (1, 3) No credentials, 30 s, no stop.
- (2, 4) The credentials, the timeout and the stop of `o` ([options](../socks5-options.md)).
- (1–2) Run the exchange on the scheduler and wait for it on the calling thread: they are for a thread, as
  [task::wait](../../async/task/wait.md) is; a task awaits (3–4).
- (3–4) The same for a task, which holds no worker while it waits.

The handshake replaces the transport's deadlines with its own, `o.timeout` from the call, and removes them when it
succeeds. A handshake that fails closes the transport, and so does a target or credentials it refuses.

## Parameters

| Parameter | Description |
|---|---|
| `transport` | the connection to the proxy |
| `target` | where the proxy connects to, `"host:port"`: a name, an IPv4 address, an IPv6 address in brackets |
| `o` | the credentials, the timeout, the stop |

## Return value

`transport`, through the proxy to the target. Or the [io::error](../../io/error/README.md), its operation `socks5`
and its path the transport's remote endpoint and the target, `proxy->target`, with the codes of
[connect](connect.md#return-value)'s handshake.

## Complexity

Two round trips to the proxy, three with username/password, the last one waiting for the proxy's connection to the
target.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> greet(net::listener l) {
    net::connection c = co_await l.async_accept();
    co_await c.async_write("hello");
    co_await c.async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(greet(l));
    net::connection proxy = net::tcp::connect("127.0.0.1:1080");
    net::connection c = net::socks5::client(proxy, l.local_endpoint().to_string());
    println("{} {}", c == proxy, c.read_all_text().value());
    c.close();
    server.wait();

    net::connection again = net::tcp::connect("127.0.0.1:1080");
    auto bad = net::socks5::client(again, "no-port");
    println("{} {}", bad.error().code() == net::errc::invalid_address, again.is_closed());
    l.close();
}
```

Output:

```text
true hello
true true
```

## See also

- [connect, async_connect](connect.md): the dial and the handshake together
- [options](../socks5-options.md): the credentials, the timeout, the stop
- [tls::client](../tls/client.md): the same for TLS over a connection there is
- [sgcl::net::socks5](README.md)
