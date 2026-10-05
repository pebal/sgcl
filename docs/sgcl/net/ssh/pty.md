[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::pty

```cpp
#include "sgcl/net/ssh/types.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    struct pty {
        string term = "xterm";
        uint32_t columns = 80;
        uint32_t rows = 24;
        uint32_t width_pixels = 0;
        uint32_t height_pixels = 0;
        vector<pair<uint8_t, uint32_t>> modes;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::pty` is a pseudo-terminal (RFC 4254 §6.2): what a client asks for before a shell
([session::request_pty](session/request_pty.md)) and what a server's handler reads
([server_session::pty](server_session/pty.md)). Its type is the program's `TERM`, its size the window's, its modes the
terminal's settings by their opcodes (RFC 4254 §8: `ECHO` 53, `ICANON` 51, `ISIG` 50, `TTY_OP_ISPEED` 128 …). Go's
`RequestPty` arguments, as one value.

## Member objects

| Member | Description |
|---|---|
| `term` | the terminal's type, `xterm` by default |
| `columns`, `rows` | its size in characters, 80 by 24 by default |
| `width_pixels`, `height_pixels` | its size in pixels; 0 by default: not given |
| `modes` | opcodes and their values; none by default: the server's own |

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
        net::ssh::pty p = s.pty().value();
        (void)s.output().write(p.term + " " + to_string(p.columns) + "x" + to_string(p.rows) + " " + to_string(p.modes.size()) + " modes");
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
    term.term = "xterm-256color";
    term.columns = 200;
    term.rows = 50;
    term.modes = {{53, 0}, {50, 1}};  // ECHO off, ISIG on
    s.request_pty(term);
    s.shell();
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
xterm-256color 200x50 2 modes
```

## See also

- [session::request_pty](session/request_pty.md), [session::window_change](session/window_change.md)
- [server_session::pty](server_session/pty.md)
- [net::ssh](README.md)
