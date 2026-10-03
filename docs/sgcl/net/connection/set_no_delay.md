[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::set_no_delay

```cpp
expected<void, io::error> set_no_delay(bool on) const noexcept;
```

Turns Nagle's algorithm off (`on`, `TCP_NODELAY`) or back on: Go's `TCPConn.SetNoDelay`. Every TCP connection the
module makes, dialed or accepted, starts with it off, as in Go: a small write goes out at once rather than wait for
more. TCP only.

## Parameters

| Parameter | Description |
|---|---|
| `on` | `true` sends small writes at once (Nagle's algorithm off), `false` lets the system gather them |

## Return value

Nothing; or the [io::error](../../io/error.md), its operation `set_no_delay`: `EOPNOTSUPP` for a unix socket or a
pair in memory, `io::errc::closed` on a connection closed, the `errno` of `setsockopt` otherwise.

## Complexity

Constant: one system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    net::connection c = net::tcp::connect(l.local_endpoint());
    println("{}", c.set_no_delay(false).has_value());

    auto [a, b] = net::connection::in_memory();
    println("{}", a.set_no_delay(true).error().message());
}
```

Output:

```text
true
set_no_delay pipe: Operation not supported on socket
```

## See also

- [set_keep_alive](set_keep_alive.md): the other TCP option
- [sgcl::net::connection](../connection.md)
