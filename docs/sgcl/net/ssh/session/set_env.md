[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::set_env, async_set_env

```cpp
expected<void, io::error> set_env(const string& name, const string& value) const;                                // (1)
async::task<expected<void, io::error>> async_set_env(const string& name, const string& value) const noexcept;    // (2)
```

An environment variable for the program (RFC 4254 §6.4), before it starts. The server decides: OpenSSH takes only
the names its AcceptEnv lists (`LANG` and `LC_*` on most systems) and refuses the rest; the module's server hands them
to its handler ([server_session::env](../server_session/env.md)).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the variable's name |
| `value` | its value |

## Return value

Nothing. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the server refuses it, `io::errc::closed` once the session is closed, the connection's error.

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
        std::string info = s.kind() == net::ssh::session_kind::shell ? "shell" : std::string(s.command().view());
        for (auto& [name, value] : s.env()) {
            info += " " + std::string(name.view()) + "=" + std::string(value.view());
        }
        if (auto p = s.pty()) {
            info += " " + std::string(p->term.view()) + " " + std::to_string(p->columns) + "x" + std::to_string(p->rows);
        }
        (void)s.output().write(string(info));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.set_env("LANG", "C");
    s.set_env("TZ", "UTC");
    s.exec("env");
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
env LANG=C TZ=UTC
```

## See also

- [sgcl::net::ssh::session](README.md)
