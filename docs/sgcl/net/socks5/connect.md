[sgcl](../../README.md) › [net](../README.md) › [socks5](README.md)

# sgcl::net::socks5::connect, async_connect

```cpp
static expected<net::connection, io::error> connect(const string& proxy, const string& target);        // (1)
static expected<net::connection, io::error> connect(const string& proxy, const string& target,         // (2)
                                                    const options& o);
static async::task<expected<net::connection, io::error>> async_connect(string proxy,                   // (3)
                                                                      string target) noexcept;
static async::task<expected<net::connection, io::error>> async_connect(string proxy, string target,    // (4)
                                                                      options o) noexcept;
```

Connects to `target` through the SOCKS5 proxy at `proxy`: the proxy dialed as [tcp::connect](../tcp/connect.md)
dials an address, its addresses raced, then the handshake of RFC 1928 over it, the proxy asked to CONNECT to the
target. What comes back is the connection to the proxy, which carries the target's bytes from then on. A name in
`target` is resolved by the proxy (`socks5h`), an address is sent as its bytes.

- (1, 3) No credentials, 30 s for the whole of it, no stop.
- (2, 4) The credentials, the timeout and the stop of `o` ([options](../socks5-options.md)): username/password
  offered beside no authentication when `o.username` is not empty, `o.timeout` bounding the dial and the
  handshake together, `o.stop` ending either.
- (1–2) Run the exchange on the scheduler and wait for it on the calling thread: they are for a thread, as
  [task::wait](../../async/task/wait.md) is; a task awaits (3–4).
- (3–4) The same for a task, which holds no worker while it waits.

The target and the credentials are checked before the proxy is dialed. The connection's deadlines are the
handshake's while it runs and none once it succeeds; a handshake that fails closes the connection.

## Parameters

| Parameter | Description |
|---|---|
| `proxy` | the proxy's `"host:port"`, as [tcp::connect](../tcp/connect.md) takes an address |
| `target` | where the proxy connects to, `"host:port"`: a name, an IPv4 address, an IPv6 address in brackets |
| `o` | the credentials, the timeout, the stop |

## Return value

The connection, through the proxy to the target. Or the [io::error](../../io/error/README.md):

- of the dial of the proxy, its operation `dial tcp` and its path the proxy's address, as
  [tcp::connect](../tcp/connect.md) reports it (`ECONNREFUSED` for a proxy that is not there);
- of the handshake, its operation `socks5` and its path `proxy->target`:
  - `net::errc::invalid_address` for a target that is not `"host:port"`, an empty host, a name past 255 bytes, an
    address with a zone; `EINVAL` for a username or password past 255 bytes, or a password without a username;
  - `net::errc::proxy_auth_required` for a proxy that takes none of the methods offered, or refused the
    credentials;
  - for the reply's REP code: `net::errc::proxy_failure` (general failure, or a code RFC 1928 does not define),
    `net::errc::proxy_refused` (not allowed by the proxy's rules), `ENETUNREACH`, `EHOSTUNREACH`, `ECONNREFUSED`,
    `ETIMEDOUT` (TTL expired), `net::errc::proxy_unsupported` (command or address type not supported);
  - `net::errc::malformed_proxy_response` for an answer that breaks the protocol: a version other than 5, a method
    never offered, an address type other than 1, 3 and 4; `io::errc::unexpected_eof` for a proxy that closed
    part way;
  - `ETIMEDOUT` (`is_timeout()`) when `o.timeout` passed, `ECANCELED` for the stop of `o.stop`.

## Complexity

The dial of the proxy, and two round trips (three with username/password) to it, the last one waiting for the
proxy's connection to the target.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> echo(net::listener l) {
    net::connection c = co_await l.async_accept();
    co_await c.async_copy_to(c);
    co_await c.async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(echo(l));
    net::connection c = net::socks5::connect("127.0.0.1:1080", l.local_endpoint().to_string());
    c.write("ping");
    c.close_write();
    println("{}", c.read_all_text().value());
    c.close();
    server.wait();

    auto bad = net::socks5::connect("127.0.0.1:1080", "no-port");
    println("{}", bad.error().code() == net::errc::invalid_address);
    l.close();
    auto refused = net::socks5::connect("127.0.0.1:1080", l.local_endpoint().to_string());
    println("{}", refused.error().code() == std::errc::connection_refused);
}
```

Output:

```text
ping
true
true
```

A task awaits the connect:

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

async::task<string> fetch(string target) {
    net::connection c = co_await net::socks5::async_connect("127.0.0.1:1080", target);
    string text = (co_await c.async_read_all_text()).value();
    co_await c.async_close();
    co_return text;
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(greet(l));
    println("{}", async::run(fetch(l.local_endpoint().to_string())));
    server.wait();
    l.close();
}
```

Output:

```text
hello
```

## See also

- [client, async_client](client.md): the handshake over a connection there is
- [options](../socks5-options.md): the credentials, the timeout, the stop
- [tcp::connect](../tcp/connect.md): how the proxy is dialed
- [sgcl::net::socks5](README.md)
