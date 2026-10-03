[sgcl](../../README.md) › [net](../README.md) › [unix_domain](README.md)

# sgcl::net::unix_domain::connect, async_connect

```cpp
static expected<net::connection, io::error> connect(const string& path);                                // (1)
static async::task<expected<net::connection, io::error>> async_connect(const string& path) noexcept;    // (2)
```

Connects to the unix socket at `path`, the file a [listen](listen.md) made: Go's `net.Dial("unix", path)`. One
address, no lookup and no race. The connection's [path](../connection/path.md) is `path`; it has no endpoints.

1. Blocks the calling thread wherever it is, a worker included, as a blocking read of `io::file` does: a worker so
   blocked runs no other task meanwhile, so a task awaits (2).
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the socket's file; at most 103 bytes on macOS, 107 on Linux |

## Return value

The [connection](../connection/README.md); or the [io::error](../../io/error/README.md), its operation `dial unix` and its path
`path`: `net::errc::invalid_address` for a path too long, `ENOENT` (`is_not_found()`) when nothing is there, the
`errno` of `connect` otherwise.

## Complexity

A few system calls.

## Exceptions

- (1) `std::system_error` when the connect has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto missing = net::unix_domain::connect("app.sock");
    println("{}", missing.error().message());
    println("{}", missing.error().is_not_found());

    net::listener l = net::unix_domain::listen("app.sock");
    net::connection c = net::unix_domain::connect("app.sock");
    println("{}", c.path());
    c.close();
    l.close();
}
```

Output:

```text
dial unix app.sock: No such file or directory
true
app.sock
```

## See also

- [listen, async_listen](listen.md): the other side
- [tcp::connect](../tcp/connect.md): over the network
- [sgcl::net::unix_domain](README.md)
