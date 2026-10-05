[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::exec, async_exec

```cpp
expected<void, io::error> exec(const string& command) const;                                // (1)
async::task<expected<void, io::error>> async_exec(const string& command) const noexcept;    // (2)
```

Starts `command` (RFC 4254 §6.5), once and after the requests: OpenSSH's server runs it in the user's shell, the
module's hands it to its handler ([server_session::command](../server_session/command.md)). Go's `Session.Start`; the
streams are read and written from here on, and [wait](wait.md) gives how it ended.

## Parameters

| Parameter | Description |
|---|---|
| `command` | the command, as the server's shell takes it |

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
    srv.handle([](net::ssh::server_session s) {
        (void)s.input().read_all();
        (void)s.output().write("ran " + s.command());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("make all");
    println("{}", s.exec("again").error().code() == net::errc::ssh_request_refused);
    s.close_input();
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
true
ran make all
```

## See also

- [client::run](../client/run.md): a command in one line
- [wait](wait.md)
- [sgcl::net::ssh::session](README.md)
