[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::set_write_deadline

```cpp
void set_write_deadline(time_point t) const noexcept;
```

Sets the deadline of the sends to `t`, an absolute time on the module's [clock](../../core/clock/README.md): Go's
`SetWriteDeadline`. A send that starts after it, or would wait past it for room, fails with `ETIMEDOUT`
(`is_timeout()`); the receives are not touched. `time_point()` removes it.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the deadline of the sends; `time_point()` for none |

## Return value

None.

## Complexity

Constant; the sends waiting are woken to look at the new deadline.

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
    net::udp::socket s = net::udp::connect("127.0.0.1:9");
    s.set_write_deadline(clock::now() - 1s);  // already past
    auto sent = s.send("late");
    println("{} {}", sent.error().is_timeout(), sent.error().op());
}
```

Output:

```text
true write
```

## See also

- [set_deadline](set_deadline.md): both directions
- [write_deadline](write_deadline.md): the deadline now
- [sgcl::net::udp::socket](README.md)
