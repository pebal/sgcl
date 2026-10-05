[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::open_session, async_open_session

```cpp
expected<session, io::error> open_session() const;                                // (1)
async::task<expected<session, io::error>> async_open_session() const noexcept;    // (2)
```

A new [session](../session/README.md) (RFC 4254 §6.1): a channel of the type `session` opened and confirmed by the
server. Nothing runs in it yet: the requests come next (a terminal, the environment), then a command, a shell or a
subsystem.

1. On a thread.
2. In a task.

## Parameters

None.

## Return value

The session. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_channel_refused` when the server refuses
it (OpenSSH's MaxSessions, the module's server's `max_sessions`) or the client has 64 channels open, the connection's
error when it has ended.

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
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("date");
    println("{}", s.output().read_all_text().value());
    println("{}", s.wait()->code);
    srv.close();
}
```

Output:

```text
ran date
0
```

## See also

- [session](../session/README.md)
- [run](run.md): a command in one line
- [sgcl::net::ssh::client](README.md)
