[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::socks5

```cpp
#include "sgcl/net/socks5.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct socks5 {
        struct options;   // the credentials, the timeout, the stop
    };
}
```

`sgcl::net::socks5` is SOCKS5, RFC 1928, with the username and password of RFC 1929: a connection to a target made
through a proxy. [connect](connect.md) dials the proxy and asks it to CONNECT to the target; [client](client.md) does
the asking over a connection to a proxy there is, one made over TLS or through another proxy. What comes back is the
connection to the proxy, which from then on carries the target's bytes: a [connection](../connection/README.md) as
any other, with TLS ([tls::client](../tls/client.md)) or HTTP on top. It is a structure of static functions, as
[tcp](../tcp/README.md) is. Go's standard library has a SOCKS5 client only inside `net/http`
(`Transport.Proxy` with a `socks5://` URL), and `golang.org/x/net/proxy` as a dialer; here it is a dial of its own,
which the [HTTP client](../http/client/README.md) uses for its `socks5://` proxies.

A target is `"host:port"`, as [tcp::connect](../tcp/connect.md) takes it. A name is sent to the proxy as it is and
resolved there, the address type `DOMAINNAME`, what curl calls `socks5h`: the program's resolver is never asked, so
a name the proxy alone can resolve (a `.onion`, a host of the proxy's network) is reached. An IPv4 address and an
IPv6 address in brackets are sent as their bytes.

## Rules

- The handshake offers no authentication, and username/password as well when [options](../socks5-options.md) has a
  username; the proxy chooses. Credentials are refused before anything is dialed when the username or the password
  is longer than 255 bytes, or there is a password without a username.
- The reply is read to its last byte and no further, so the target's first bytes, sent behind the reply, are the
  program's to read.
- `options::timeout` bounds the dial of the proxy and the handshake together, 30 s by default; a stop token ends
  either with `ECANCELED`. The connection's deadlines are the handshake's while it runs and are removed when it
  succeeds.
- A handshake that fails closes the connection, the transport given to `client` included.
- The REP code of a reply that is not success is the error: a target refused, unreachable or out of time through
  the proxy reads as one refused, unreachable or out of time directly (`ECONNREFUSED`, `EHOSTUNREACH`,
  `ENETUNREACH`, `ETIMEDOUT`), the others as net's own codes ([errc](../errc.md)).
- The blocking forms run the exchange on the scheduler and wait for it, so they are for a thread, as
  [task::wait](../../async/task/wait.md) is; a task awaits the `async_` forms.

## Member types

| Type | Definition |
|---|---|
| [options](../socks5-options.md) | the credentials, the timeout and the stop of a connection |

## Member functions

| Function | Description |
|---|---|
| [connect, async_connect](connect.md) | a connection to a target through a proxy (static) |
| [client, async_client](client.md) | the handshake over a connection to a proxy there is (static) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> greet(net::listener l) {
    net::connection c = co_await l.async_accept();
    co_await c.async_write("hello through the proxy");
    co_await c.async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(greet(l));
    // the proxy at 127.0.0.1:1080 asked for the listener's address
    net::connection c = net::socks5::connect("127.0.0.1:1080", l.local_endpoint().to_string());
    println("{}", c.read_all_text().value());
    c.close();
    server.wait();
    l.close();
}
```

Output:

```text
hello through the proxy
```

## See also

- [connection](../connection/README.md): what it makes
- [tcp](../tcp/README.md): the connections it is made of
- [http::client](../http/client/README.md): its `socks5://` and `socks5h://` proxies
- [errc](../errc.md): `proxy_failure`, `proxy_refused`, `proxy_unsupported`, `proxy_auth_required`,
  `malformed_proxy_response`
- RFC 1928, RFC 1929; `tests/net/socks5.cpp`, against a server written from them and with curl and Go
