[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::set_read_deadline

```cpp
void set_read_deadline(time_point t) const noexcept;
```

Sets the deadline of the reads to `t`, an absolute time on the module's [clock](../../core/clock.md): Go's
`Conn.SetReadDeadline`. A read that starts after it, or would wait past it, fails with `ETIMEDOUT` (`is_timeout()`)
and takes nothing, even when data is there; the writes are not touched. `time_point()` removes it. A change applies
to the reads in progress: a deadline in the past, set from another task, ends a read that waits at once.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the deadline of the reads; `time_point()` for none |

## Return value

None.

## Complexity

Constant; the reads waiting are woken to look at the new deadline.

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

async::task<> wait_for_data(net::connection c) {
    byte buffer[16];
    auto r = co_await c.async_read(buffer);  // nothing comes
    println("{} {}", r.error().message(), r.error().is_timeout());
}

int main() {
    auto [a, b] = net::connection::in_memory();
    auto waiting = async::spawn(wait_for_data(b));
    async::sleep(10ms).wait();
    b.set_read_deadline(clock::now() - 1s);  // in the past: the read ends now
    waiting.wait();
}
```

Output:

```text
read pipe: Operation timed out true
```

## See also

- [set_deadline](set_deadline.md): both directions, and how a deadline differs from a timeout
- [read_deadline](read_deadline.md): the deadline now
- [sgcl::net::connection](../connection.md)
