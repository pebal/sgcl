[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::set_deadline

```cpp
void set_deadline(time_point t) const noexcept;
```

Sets the deadline of both directions to `t`, an absolute time on the module's [clock](../../core/clock/README.md): Go's
`SetDeadline`. It is [set_read_deadline](set_read_deadline.md) and [set_write_deadline](set_write_deadline.md)
together. A receive or a send that starts after the deadline of its direction, or would wait past it, fails with
`ETIMEDOUT` (`is_timeout()`), even when a datagram is there. `time_point()` removes the deadline. A change applies
to the operations in progress too.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the deadline; `time_point()` for none |

## Return value

None.

## Complexity

Constant; the operations waiting are woken to look at the new deadline.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    s.set_deadline(clock::now() + 20ms);
    vector<byte> room(64);
    auto d = s.receive_from(room);  // nobody sends
    println("{} {}", d.error().is_timeout(), d.error().op());

    s.set_deadline(time_point());
    println("{}", s.read_deadline() == time_point());
}
```

Output:

```text
true read
true
```

## See also

- [set_read_deadline](set_read_deadline.md), [set_write_deadline](set_write_deadline.md): one direction
- [connection::set_deadline](../connection/set_deadline.md): the deadlines of a stream, the same rules
- [sgcl::net::udp::socket](README.md)
