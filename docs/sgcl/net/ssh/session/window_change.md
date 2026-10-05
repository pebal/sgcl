[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::window_change, async_window_change

```cpp
expected<void, io::error> window_change(uint32_t columns, uint32_t rows) const;                                // (1)
async::task<expected<void, io::error>> async_window_change(uint32_t columns, uint32_t rows) const noexcept;    // (2)
```

The terminal's new size (RFC 4254 §6.7), after [request_pty](request_pty.md), when the program's window changes:
OpenSSH's server passes it to the terminal (SIGWINCH to the program), the module's to its handler's
[pty](../server_session/pty.md). No answer is asked for, so it returns once it is sent.

## Parameters

| Parameter | Description |
|---|---|
| `columns` | the width in characters |
| `rows` | the height in characters |

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
    s.request_pty();
    s.shell();
    s.window_change(120, 40);
    s.input().write("size\n");
    auto out = io::buffered_reader(s.output());
    println("{}", out.read_line().value().value());
    srv.close();
}
```

Output:

```text
120x40
```

## See also

- [request_pty](request_pty.md), [pty](../pty.md)
- [sgcl::net::ssh::session](README.md)
