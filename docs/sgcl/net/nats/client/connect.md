[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& url);                                                   // (1)
static async::task<expected<client, io::error>> async_connect(string url) noexcept;                              // (2)
static expected<client, io::error> connect(const string& url, const options& o);                                 // (3)
static async::task<expected<client, io::error>> async_connect(string url, options o) noexcept;                   // (4)
static expected<client, io::error> connect(const net::connection& transport, const options& o);                  // (5)
static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept;    // (6)
```

A connection to the server: the dial, its INFO, TLS for `tls://` or a server that requires it, CONNECT with the
credentials (the nonce signed with an nkey), and a PING whose PONG says the server took them.

- (1–4) The server of the URL: `nats://[user:password@]host[:4222]`, `nats://token@host`, `tls://host`, or
  `host:port`; the URL's credentials when the options have none.
- (5–6) Over a connection the program made.

The options' timeout bounds the dial, TLS and CONNECT together; their stop ends the dial with `ECANCELED`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | where the server is, and the credentials |
| `transport` | a connection to it |
| `o` | the credentials, TLS, PINGs, the timeout, the queue of messages |


## Return value

The connection; the dial's error, TLS's, `errc::authorization_violation` for credentials refused, `errc::malformed`
for a peer that is no NATS server, `net::errc::invalid_url` or `net::errc::unsupported_scheme` for a URL that is not
one.

## Complexity

A few round trips.

## Exceptions

- (1), (3), (5) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4), (6) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    auto nc = net::nats::client::connect("nats://localhost:4222");
    println("{} {}", bool(nc), nc->server_id());
}
```

Output:

```text
true NTEST
```

## See also

- [options](../client-options.md)
- [client](README.md)
