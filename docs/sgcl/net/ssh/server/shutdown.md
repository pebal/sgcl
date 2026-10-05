[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server](README.md)

# sgcl::net::ssh::server::shutdown, async_shutdown

```cpp
void shutdown() const;                            // (1)
async::task<> async_shutdown() const noexcept;    // (2)
```

The listeners closed, so that no connection is accepted, then the connections waited for until they end: SSH has no
graceful end of its own, so a connection ends when its client closes it. A limit on the wait is
`co_await async::with_timeout(s.async_shutdown(), 10s)`, then [close](close.md).

1. On a thread.
2. In a task.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the connections.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
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
    thread closing([c] { c.close(); });  // the client ends its connection
    srv.shutdown();
    closing.join();
    println("{}", c.is_closed());
    srv.close();
}
```

Output:

```text
true
```

## See also

- [close](close.md)
- [sgcl::net::ssh::server](README.md)
