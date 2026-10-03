[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::set_deadline

```cpp
void set_deadline(time_point t) const noexcept;
```

Sets the deadline of both directions to `t`, an absolute time on the module's [clock](../../core/clock/README.md): Go's
`Conn.SetDeadline`. It is [set_read_deadline](set_read_deadline.md) and [set_write_deadline](set_write_deadline.md)
together. A read or a write that starts after the deadline of its direction, or would wait past it, fails with
`ETIMEDOUT` (`is_timeout()`) and takes nothing, even when data is there. `time_point()` removes the deadline.

A change applies to the operations in progress too: a deadline in the past, set from another task, ends a read that
waits at once. The deadline is a time, not a duration: a limit on each operation is
`c.set_read_deadline(clock::now() + 5s)` before each, and a limit on a whole conversation one deadline before it.

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

## Notes

`async::timeout(c.async_read(b), 5s)` is not the same as a deadline: the read that lost the race to the timer runs
on, and takes the data when it comes. A deadline ends the read itself. The deadlines are on the module's clock, so a
test moves them with a [manual_clock](../../async/manual_clock/README.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    auto [a, b] = net::connection::in_memory();
    b.set_deadline(clock::now() + 20ms);
    byte buffer[16];
    auto r = b.read(buffer);  // nothing comes
    println("{} {}", r.error().message(), r.error().is_timeout());
    auto w = b.write("nobody reads");
    println("{}", w.error().is_timeout());

    b.set_deadline(time_point());
    println("{}", b.read_deadline() == time_point());
}
```

Output:

```text
read pipe: Operation timed out true
true
true
```

## See also

- [set_read_deadline](set_read_deadline.md), [set_write_deadline](set_write_deadline.md): one direction
- [read_deadline](read_deadline.md), [write_deadline](write_deadline.md): the deadlines now
- [close](close.md): an end to the waits now
- [sgcl::net::connection](README.md)
