[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::request_pty, async_request_pty

```cpp
expected<void, io::error> request_pty(const pty& p = {}) const;                                // (1)
async::task<expected<void, io::error>> async_request_pty(const pty& p = {}) const noexcept;    // (2)
```

A pseudo-terminal for the program (RFC 4254 §6.2), before it starts: its type (`TERM`), its size and its modes. A
program run in a terminal sees one (`tty`, a shell's prompt, an editor), and its output and error come merged on
[output](output.md), as a terminal shows them.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the terminal: its type, size and modes ([pty](../pty.md)); `xterm` of 80 by 24 by default |

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
    net::ssh::pty term;
    term.term = "vt100";
    term.columns = 132;
    term.rows = 43;
    s.request_pty(term);
    s.shell();
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
shell vt100 132x43
```

## See also

- [pty](../pty.md)
- [window_change](window_change.md)
- [sgcl::net::ssh::session](README.md)
