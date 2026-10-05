[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::exit_signal, async_exit_signal

```cpp
expected<void, io::error> exit_signal(const string& name, bool core_dumped = false,                       // (1)
                                      const string& message = {}) const;
async::task<expected<void, io::error>> async_exit_signal(const string& name, bool core_dumped = false,    // (2)
                                                         const string& message = {}) const noexcept;
```

The program's end by a signal (RFC 4254 §6.10) instead of a status: the signal's name without `SIG`, whether it dumped
core, a message for the user; once, as [exit](exit.md) is. The client's [wait](../session/wait.md) gives code -1 and
the signal.

1. On a thread (a handler returning `void`).
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the signal's name, `TERM`, `KILL`, `SEGV` … |
| `core_dumped` | whether the program dumped core |
| `message` | a text for the user |

## Return value

Nothing; or the connection's error.

## Complexity

Constant.

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
    srv.handle([](net::ssh::server_session s) { (void)s.exit_signal("KILL", false, "killed by the handler"); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::exit_status st = c.run("job")->status;
    println("{} {} {}", st.code, st.signal, st.message);
    srv.close();
}
```

Output:

```text
-1 KILL killed by the handler
```

## See also

- [exit](exit.md)
- [exit_status](../exit_status.md)
- [sgcl::net::ssh::server_session](README.md)
