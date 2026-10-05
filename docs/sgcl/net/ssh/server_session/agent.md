[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::agent, async_agent

```cpp
expected<ssh::agent, io::error> agent() const;                                // (1)
async::task<expected<ssh::agent, io::error>> async_agent() const noexcept;    // (2)
```

The client's agent, when the client forwards it (`ssh -A`, [client::options](../client-options.md)'s `forward_agent`):
an `auth-agent@openssh.com` channel opened to the client, which connects it to its own agent, given as an
[agent](../agent/README.md) of this side: its keys listed, signatures made by them for a connection the handler makes
onwards. The channels opened this way are closed when the session ends.

1. On a thread (a handler returning `void`).
2. In a task.

## Parameters

None.

## Return value

The agent. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the client did not ask
for its agent's forwarding, `net::errc::ssh_channel_refused` when it refuses the channel (it has no agent), the
connection's error.

## Complexity

A round trip.

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
    srv.handle([](net::ssh::server_session s) {
        auto a = s.agent();
        bool refused = !a && a.error().code() == net::errc::ssh_request_refused;
        (void)s.output().write(a ? "forwarded" : refused ? "not forwarded" : "no agent");
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("make test")->out);
    srv.close();
}
```

Output:

```text
not forwarded
```

## See also

- [agent](../agent/README.md)
- [client::options](../client-options.md): `forward_agent`
- [sgcl::net::ssh::server_session](README.md)
