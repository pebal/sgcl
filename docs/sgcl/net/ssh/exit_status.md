[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::exit_status

```cpp
#include "sgcl/net/ssh/types.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    struct exit_status {
        int code = -1;
        string signal;
        bool core_dumped = false;
        string message;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::exit_status` is how a remote program ended (RFC 4254 §6.10): its exit code, or the signal that ended it,
whether it dumped core and the server's message about it. What [session::wait](session/wait.md) gives and
[run_result](run_result.md) holds; what a server's handler sends by [exit](server_session/exit.md) or
[exit_signal](server_session/exit_signal.md). Go's `ExitError` and its `ExitStatus`, `Signal`, `Msg`, as a value
rather than an error.

## Member objects

| Member | Description |
|---|---|
| `code` | the exit code, 0 for success; -1 when a signal ended the program or the server sent neither |
| `signal` | the signal's name without `SIG` (`TERM`, `SEGV`); empty when it exited with a code |
| `core_dumped` | whether it dumped core |
| `message` | the server's message about the signal; often empty |

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
        if (s.command() == "abort") {
            (void)s.exit_signal("ABRT", true);
        } else {
            (void)s.exit(4);
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    for (const char* command : {"grep", "abort"}) {
        net::ssh::exit_status st = c.run(command)->status;
        println("{}: code {} signal [{}] core {}", command, st.code, st.signal, st.core_dumped);
    }
    srv.close();
}
```

Output:

```text
grep: code 4 signal [] core false
abort: code -1 signal [ABRT] core true
```

## See also

- [session::wait](session/wait.md), [run_result](run_result.md)
- [server_session::exit](server_session/exit.md), [exit_signal](server_session/exit_signal.md)
- [net::ssh](README.md)
