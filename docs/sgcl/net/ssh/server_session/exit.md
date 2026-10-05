[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::exit, async_exit

```cpp
expected<void, io::error> exit(int code) const;                                // (1)
async::task<expected<void, io::error>> async_exit(int code) const noexcept;    // (2)
```

The program's exit status (RFC 4254 §6.10), sent after the output written so far; once: a second, or an
[exit_signal](exit_signal.md) after it, is ignored. The session goes on until the handler returns (its output can
still be written, though a client that waits for the status may stop reading), and a handler that sends none returns
with status 0.

1. On a thread (a handler returning `void`).
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the exit status, 0 for success |

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
    srv.handle([](net::ssh::server_session s) { (void)s.exit(s.command() == "fail" ? 1 : 0); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{} {}", c.run("fail")->status.code, c.run("pass")->status.code);
    srv.close();
}
```

Output:

```text
1 0
```

## See also

- [exit_signal](exit_signal.md)
- [session::wait](../session/wait.md)
- [sgcl::net::ssh::server_session](README.md)
