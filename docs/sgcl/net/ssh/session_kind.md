[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::session_kind

```cpp
#include "sgcl/net/ssh/server.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    enum class session_kind : uint8_t {
        exec,
        shell,
        subsystem,
    };
}
```

What a session runs (RFC 4254 §6.5), as the client started it and a server's handler reads it
([server_session::kind](server_session/kind.md)).

| Value | Description |
|---|---|
| `exec` | a command ([session::exec](session/exec.md)), in [server_session::command](server_session/command.md) |
| `shell` | the user's shell ([session::shell](session/shell.md)) |
| `subsystem` | a subsystem by name ([session::subsystem](session/subsystem.md)), in [server_session::subsystem](server_session/subsystem.md) |

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
        bool command = s.kind() == net::ssh::session_kind::exec;
        (void)s.output().write(command ? "a command: " + s.command() : string("something else"));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("df -h")->out);
    srv.close();
}
```

Output:

```text
a command: df -h
```

## See also

- [server_session::kind](server_session/kind.md)
- [net::ssh](README.md)
