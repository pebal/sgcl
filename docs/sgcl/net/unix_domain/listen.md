[sgcl](../../README.md) › [net](../README.md) › [unix_domain](README.md)

# sgcl::net::unix_domain::listen, async_listen

```cpp
static expected<net::listener, io::error> listen(const string& path) noexcept;                       // (1)
static async::task<expected<net::listener, io::error>> async_listen(const string& path) noexcept;    // (2)
```

Creates a unix socket at `path` and listens on it with the system's backlog: Go's `net.Listen("unix", path)`. The
socket's file is created here, and anything at the path already is an error, as in Go; the listener's
[close](../listener/close.md) removes the file. The listener's [path](../listener/path.md) is `path`.

1. On the calling thread; nothing waits.
2. The same for a task, for symmetry: there is nothing to wait for.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the socket's file; at most 103 bytes on macOS, 107 on Linux |

## Return value

The [listener](../listener/README.md); or the [io::error](../../io/error/README.md), its operation `listen unix` and its path
`path`: `net::errc::invalid_address` for a path too long, `EADDRINUSE` when something is at the path, the `errno`
of `bind` or `listen` otherwise.

## Complexity

A few system calls.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::listener l = net::unix_domain::listen("app.sock");
    auto again = net::unix_domain::listen("app.sock");
    println("{}", again.error().message());

    auto long_path = net::unix_domain::listen(string(120, 'a'));
    println("{}", long_path.error().code() == net::errc::invalid_address);

    l.close();
    println("{}", io::exists("app.sock"));
}
```

Output:

```text
listen unix app.sock: Address already in use
true
false
```

## See also

- [connect, async_connect](connect.md): the other side
- [listener](../listener/README.md): what it gives
- [sgcl::net::unix_domain](README.md)
