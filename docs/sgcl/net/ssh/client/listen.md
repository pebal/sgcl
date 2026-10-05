[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::listen, async_listen

```cpp
expected<net::listener, io::error> listen(const string& address) const;                                // (1)
async::task<expected<net::listener, io::error>> async_listen(const string& address) const noexcept;    // (2)
```

A listener on the server's side at `address`, `"host:port"` (tcpip-forward, RFC 4254 §7.1): `ssh -R`'s way, the server
listening and each connection it accepts coming here as a channel. What comes back is a
[net::listener](../../listener/README.md) like any other: its `accept` gives the connections, its `close` cancels the
forwarding at the server. Port 0 lets the server choose, and `local_endpoint()` holds the port it chose; an empty host
is every address of the server's, `localhost` its loopback (OpenSSH binds every address only when its GatewayPorts
allows it).

1. On a thread.
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where the server listens, `"host:port"` |

## Return value

The listener. Or the [io::error](../../../io/error/README.md): `net::errc::invalid_address` for an address that is not
`"host:port"`, `net::errc::ssh_request_refused` when the server refuses (its rules, a port taken), the connection's
error.

## Complexity

A round trip; then a channel opened by the server for each connection.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.allow_tcpip_forward = [](const string& user, const string& host, uint16_t port) { return true; };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::listener remote = c.listen("127.0.0.1:0");
    net::endpoint at(net::ip_address("127.0.0.1"), remote.local_endpoint().port());
    net::connection outside = net::tcp::connect(at);  // to the server's port
    outside.write("knock");
    outside.close_write();
    net::connection in = remote.accept();
    println("{}", in.read_all_text().value());
    remote.close();
    srv.close();
}
```

Output:

```text
knock
```

## See also

- [dial](dial.md): the other way
- [server](../server/README.md): `allow_tcpip_forward`
- [net::listener](../../listener/README.md)
- [sgcl::net::ssh::client](README.md)
