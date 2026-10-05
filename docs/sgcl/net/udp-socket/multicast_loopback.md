[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::multicast_loopback

```cpp
expected<bool, io::error> multicast_loopback() const noexcept;
```

Returns whether the socket's datagrams to a multicast group reach this machine's members too: what
[set_multicast_loopback](set_multicast_loopback.md) set, `true` before.

## Parameters

None.

## Return value

The setting; or the [io::error](../../io/error/README.md), its operation `multicast loopback`: `io::errc::closed` for a
closed socket, the system's `errno` otherwise.

## Complexity

One system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::udp::socket sender = net::udp::bind("[::1]:0");
    println("{}", sender.multicast_loopback().value());
    sender.set_multicast_loopback(false);
    println("{}", sender.multicast_loopback().value());
}
```

Output:

```text
true
false
```

## See also

- [set_multicast_loopback](set_multicast_loopback.md): the other way
- [udp::socket](README.md)
