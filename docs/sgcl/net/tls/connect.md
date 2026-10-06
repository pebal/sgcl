[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::connect, async_connect

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    expected<net::connection, io::error> connect(const string& address,                         // (1)
                                                 const config& c = {});
    async::task<expected<net::connection, io::error>> async_connect(string address,             // (2)
                                                                    config c = {}) noexcept;
}
```

Dials `address` over TCP and completes the client's handshake over it, and only then gives the connection out:
Go's `tls.Dial` and `tls.DialWithDialer`, which also complete the handshake before they return (Go's `tls.Client`
waits for the first read or write). The address is taken as [tcp::connect](../tcp/connect.md) takes it,
`"host:port"`, and dialed with happy eyeballs. The server's chain is verified against `c.roots`, or the system's
roots, for `c.server_name`, or the host of `address` when it has none ([config](config.md)). The lookup, the
TCP connect and the handshake share one bound, `c.handshake_timeout`.

1. On the calling thread: it runs on the scheduler and waits for it, so it is for a thread.
2. The same for a task, which holds no worker while it waits.

The connection is a [net::connection](../connection/README.md) like a TCP one, and its errors name it `tls` and the TCP
connection (`read tls tcp 127.0.0.1:50000->127.0.0.1:8443: ...`); [state_of](state_of.md) gives what the handshake
settled.

## Parameters

| Parameter | Description |
|---|---|
| `address` | `"host:port"`: a name, an IPv4 address or an IPv6 address in brackets, and a port number |
| `c` | the settings: the roots, the name, the groups, the cipher suites, the ALPN protocols, the timeout |

## Return value

The connection, its handshake done and the server's chain verified. Or the [io::error](../../io/error/README.md):

- [net::errc::invalid_address](../errc.md) for an address that is not `"host:port"`; what
  [tcp::connect](../tcp/connect.md) returns for a name not found, a refused connection;
- `EINVAL` for a config the handshake cannot start with: no group or no cipher suite, no name to verify (none given
  and an address without a host), an ALPN protocol of 0 or more than 255 bytes;
- `ETIMEDOUT` when `c.handshake_timeout` passed (`is_timeout()`);
- an error of the category `"tls"` for an alert of either side or a chain that did not verify ([alert_of](alert_of.md),
  [certificate_reason](certificate_reason.md)); `alert::ech_required` for a `c.ech_config_list` the server took
  neither the first time nor, dialed once more, with the `retry_configs` it sent.

## Complexity

The lookup and the connect of [tcp::connect](../tcp/connect.md), then one round trip of the handshake, two when
the server asks for another group with a HelloRetryRequest; the verification of the chain.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

A server of the tree's test certificate (`tests/net/tls_testdata`: a leaf for localhost, signed by a CA of its own)
and its client, which trusts that CA rather than the system's roots:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> greet(net::listener incoming) {
    auto c = co_await incoming.async_accept();  // its handshake done
    if (c) {
        co_await c->async_write(string("hello over tls\n"));
        co_await c->async_close();
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    auto serving = async::spawn(greet(incoming));

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    if (!c) {
        eprintln("{}", c.error().message());
        return 1;
    }
    println("{}", **c->read_line());
    c->close();
    serving.wait();
    incoming.close();
}
```

Output:

```text
hello over tls
```

A task that dials with the default config: the system's roots do not know the test CA, and the chain is refused.

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> dial(string address) {
    auto c = co_await net::tls::async_connect(address);
    if (!c) {
        println("{}", c.error().code().message());
        println("{}", net::tls::certificate_reason(c.error()) ==
                          crypto::x509::reason::unknown_authority);
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    async::run(dial("localhost:" + to_string(incoming.local_endpoint().port())));
    incoming.close();
}
```

Output:

```text
tls: certificate signed by unknown authority
true
```

## See also

- [client, async_client](client.md): the same handshake over a connection there is
- [config](config.md): the settings; [state_of](state_of.md): what the handshake settled
- [tcp::connect](../tcp/connect.md): the connection under it
- [net::tls](README.md)
