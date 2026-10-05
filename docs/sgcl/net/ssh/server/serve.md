[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server](README.md)

# sgcl::net::ssh::server::serve, async_serve

```cpp
expected<void, io::error> serve(const string& address) const;                                 // (1)
expected<void, io::error> serve(const net::listener& l) const;                                // (2)
async::task<expected<void, io::error>> async_serve(const string& address) const noexcept;     // (3)
async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept;    // (4)
```

Serves until [shutdown](shutdown.md) or [close](close.md): each connection accepted is served in a task of its own —
the version lines, the key exchange with the host keys, the authentication by the callbacks within
`login_grace_time`, then its channels: sessions handed to the handler, forwarding the callbacks allow.

- (1, 3) Listens on `address` (`":2222"`, `"127.0.0.1:0"`), as [tcp::listen](../../tcp/listen.md) does.
- (2, 4) The connections of a listener the program made (one of a port it chose, a unix socket's).
- (1–2) Block the calling thread (main's), the connections served on the scheduler meanwhile.
- (3–4) For a task.

The fields are read when it is called.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where to listen, `"host:port"` |
| `l` | a listener whose connections are served |

## Return value

`net::errc::server_closed` after `shutdown` or `close`, as Go's `ErrServerClosed`. Or the
[io::error](../../../io/error/README.md) of the listen or of an accept that will not pass; `EINVAL` for a server without a
host key.

## Complexity

Linear in the connections served.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ssh::server srv;
    auto none = srv.serve("127.0.0.1:0");
    println("{}", none.error().code() == std::errc::invalid_argument);

    srv.host_keys = {net::ssh::private_key::generate()};
    srv.no_client_auth = true;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    srv.shutdown();
    println("{}", serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
true
true
```

## See also

- [shutdown](shutdown.md), [close](close.md)
- [sgcl::net::ssh::server](README.md)
