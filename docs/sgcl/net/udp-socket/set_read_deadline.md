[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::set_read_deadline

```cpp
void set_read_deadline(time_point t) const noexcept;
```

Sets the deadline of the receives to `t`, an absolute time on the module's [clock](../../core/clock/README.md): Go's
`SetReadDeadline`. A receive that starts after it, or would wait past it, fails with `ETIMEDOUT` (`is_timeout()`);
the sends are not touched. `time_point()` removes it. A change applies to the receives in progress: a deadline in
the past, set from another task, ends a receive that waits at once.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the deadline of the receives; `time_point()` for none |

## Return value

None.

## Complexity

Constant; the receives waiting are woken to look at the new deadline.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> wait_for_one(net::udp::socket s) {
    vector<byte> room(64);
    auto d = co_await s.async_receive_from(room);  // nobody sends
    println("{}", d.error().is_timeout());
}

int main() {
    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    auto waiting = async::spawn(wait_for_one(s));
    async::sleep(10ms).wait();
    s.set_read_deadline(clock::now() - 1s);  // in the past: the receive ends now
    waiting.wait();
}
```

Output:

```text
true
```

## See also

- [set_deadline](set_deadline.md): both directions
- [read_deadline](read_deadline.md): the deadline now
- [sgcl::net::udp::socket](README.md)
