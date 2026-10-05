[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::pty

```cpp
optional<pty> pty() const noexcept;
```

The terminal the client asked for before the start: its type, its size in characters and pixels and its modes ([pty](../pty.md)), the size as the client's last window change set it. `nullopt` when it asked for none.

## Parameters

None.

## Return value

The terminal, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

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
        if (auto p = s.pty()) {
            (void)s.output().write(p->term + " " + to_string(p->columns) + "x" + to_string(p->rows));
        } else {
            (void)s.output().write("no terminal");
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("tty")->out);
    net::ssh::session s = c.open_session();
    s.request_pty();
    s.exec("tty");
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
no terminal
xterm 80x24
```

## See also

- [pty](../pty.md)
- [session::request_pty](../session/request_pty.md)
- [sgcl::net::ssh::server_session](README.md)
