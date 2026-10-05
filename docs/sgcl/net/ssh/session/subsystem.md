[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::subsystem, async_subsystem

```cpp
expected<void, io::error> subsystem(const string& name) const;                                // (1)
async::task<expected<void, io::error>> async_subsystem(const string& name) const noexcept;    // (2)
```

Starts a subsystem by name (RFC 4254 §6.5), once: `sftp`, which a file transfer client speaks over the session's
streams, or a name the server defines. Go's `Session.RequestSubsystem`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the subsystem's name |

## Return value

Nothing. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the server refuses it or something already started, `io::errc::closed` once the session is closed, the connection's error.

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
#include <string>

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("subsystem " + s.subsystem()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.subsystem("sftp");
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
subsystem sftp
```

## See also

- [server_session::subsystem](../server_session/subsystem.md)
- [wait](wait.md)
- [sgcl::net::ssh::session](README.md)
