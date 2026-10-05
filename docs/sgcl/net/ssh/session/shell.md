[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::shell, async_shell

```cpp
expected<void, io::error> shell() const;                                // (1)
async::task<expected<void, io::error>> async_shell() const noexcept;    // (2)
```

Starts the user's login shell (RFC 4254 §6.5), once and after the requests: its commands are written to
[input](input.md), usually after a [request_pty](request_pty.md), as `ssh host` does. Go's `Session.Shell`.

## Parameters

None.

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
        auto lines = io::buffered_reader(s.input());
        while (auto line = lines.read_line().value()) {
            (void)s.output().write("$ " + string(*line) + "\n");
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.shell();
    s.input().write("cd /tmp\nls\n");
    s.close_input();
    print("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
$ cd /tmp
$ ls
```

## See also

- [request_pty](request_pty.md)
- [wait](wait.md)
- [sgcl::net::ssh::session](README.md)
