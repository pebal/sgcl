[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::wait, async_wait

```cpp
expected<void, io::error> wait() const;                                // (1)
async::task<expected<void, io::error>> async_wait() const noexcept;    // (2)
```

Waits until the connection ends: the server's disconnect, a failure, a [close](close.md) from any task or thread. A
program that keeps a connection for its forwarded ports waits here, as `ssh -N` does.

1. On a thread.
2. In a task.

## Parameters

None.

## Return value

Always the [io::error](../../../io/error/README.md) that ended the connection: `io::errc::closed` after `close`,
`net::errc::ssh_disconnected` for the server's disconnect (its reason in the message), the transport's error.

## Complexity

Constant: woken by the end.

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
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    srv.close();
    auto ended = c.wait();
    println("{}", c.is_closed());
    srv.close();
}
```

Output:

```text
true
```

## See also

- [close](close.md), [is_closed](is_closed.md)
- [sgcl::net::ssh::client](README.md)
