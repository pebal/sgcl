[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::set_keep_alive

```cpp
expected<void, io::error> set_keep_alive(duration idle) const noexcept;
```

Sends keep-alive probes after `idle` of silence, or none when `idle` is zero: Go's `TCPConn.SetKeepAlive` and
`SetKeepAlivePeriod` in one call. `idle` is rounded up to whole seconds, at least one; the probes then go `idle`
apart, nine of them before the connection is given up. Every TCP connection the module makes, dialed or accepted,
starts with probes after 15 s, as in Go. TCP only.

## Parameters

| Parameter | Description |
|---|---|
| `idle` | the silence before the first probe and between the probes; zero turns them off |

## Return value

Nothing; or the [io::error](../../io/error/README.md), its operation `set_keep_alive`: `EOPNOTSUPP` for a unix socket or a
pair in memory, `io::errc::closed` on a connection closed, the `errno` of `setsockopt` otherwise.

## Complexity

Constant: a few system calls.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    net::connection c = net::tcp::connect(l.local_endpoint());
    println("{}", c.set_keep_alive(30s).has_value());
    println("{}", c.set_keep_alive(0s).has_value());  // no probes
    c.close();
    println("{}", c.set_keep_alive(30s).error().is_closed());
}
```

Output:

```text
true
true
true
```

## See also

- [set_no_delay](set_no_delay.md): the other TCP option
- [set_deadline](set_deadline.md): a limit on a silence the program waits through
- [sgcl::net::connection](README.md)
