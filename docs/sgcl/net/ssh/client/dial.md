[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::dial, async_dial

```cpp
expected<net::connection, io::error> dial(const string& address) const;                                // (1)
async::task<expected<net::connection, io::error>> async_dial(const string& address) const noexcept;    // (2)
```

A connection to `address`, `"host:port"`, made by the server (direct-tcpip, RFC 4254 §7.2): `ssh -L`'s way, the server
dialing and the bytes going through the SSH connection. What comes back is a
[net::connection](../../connection/README.md) like any other: `read`, `write`, deadlines, `close_write` (the channel's
EOF), `close`, the `async_` forms, so that TLS, HTTP and every stream of io take it. Its remote endpoint is the
address when the host is an address.

1. On a thread.
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where the server connects to, `"host:port"`; the name is resolved by the server |

## Return value

The connection. Or the [io::error](../../../io/error/README.md): `net::errc::invalid_address` for an address that is
not `"host:port"`, `net::errc::ssh_channel_refused` when the server refuses (its rules, a port nobody listens on: the
message says which), the connection's error.

## Complexity

A round trip, and the server's connection to the address.

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
    srv.allow_direct_tcpip = [](const string& user, const string& host, uint16_t port) { return true; };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::connection through = c.dial(l.local_endpoint().to_string());  // the SSH server itself
    println("{}", through.read_line().value().value());
    srv.close();
}
```

Output:

```text
SSH-2.0-SGCL_1.0
```

## See also

- [listen](listen.md): the other way
- [server](../server/README.md): `allow_direct_tcpip`
- [net::connection](../../connection/README.md)
- [sgcl::net::ssh::client](README.md)
