[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::signal, async_signal

```cpp
expected<void, io::error> signal(const string& name) const;                                // (1)
async::task<expected<void, io::error>> async_signal(const string& name) const noexcept;    // (2)
```

A signal to the program (RFC 4254 §6.9) by its name without `SIG`: `TERM`, `INT`, `HUP`, `KILL`, `QUIT`, `USR1` …
No answer is asked for. OpenSSH's server delivers it since its version 7.9; the module's records it for its handler
([last_signal](../server_session/last_signal.md)).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the signal's name, without `SIG` |

## Return value

Nothing. Or the [io::error](../../../io/error/README.md): `io::errc::closed` once the session is closed, the connection's error.

## Complexity

Constant: sent, no answer awaited.

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
        auto lines = io::buffered_reader(s.input());
        for (;;) {
            auto line = lines.read_line();
            if (!line || !*line) {
                break;  // the end of the input, or of the session
            }
            if (**line == "size") {
                (void)s.output().write(to_string(s.pty()->columns) + "x" + to_string(s.pty()->rows) + "\n");
            } else {
                (void)s.output().write(s.last_signal() + "\n");
            }
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
    s.exec("server");
    s.signal("HUP");
    s.input().write("signal?\n");
    auto out = io::buffered_reader(s.output());
    println("{}", out.read_line().value().value());
    srv.close();
}
```

Output:

```text
HUP
```

## See also

- [wait](wait.md): an end by a signal
- [sgcl::net::ssh::session](README.md)
