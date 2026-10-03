[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::set_write_deadline

```cpp
void set_write_deadline(time_point t) const noexcept;
```

Sets the deadline of the writes to `t`, an absolute time on the module's [clock](../../core/clock.md): Go's
`Conn.SetWriteDeadline`. A write that starts after it, or would wait past it for room, fails with `ETIMEDOUT`
(`is_timeout()`); the reads are not touched. `time_point()` removes it. A change applies to the writes in progress.
A write that fails part way reports the error alone, and the connection is then of no use but to close.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the deadline of the writes; `time_point()` for none |

## Return value

None.

## Complexity

Constant; the writes waiting are woken to look at the new deadline.

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
    auto [a, b] = net::connection::in_memory();
    a.set_write_deadline(clock::now() + 20ms);
    auto w = a.write("nobody reads it");  // a pair in memory waits for the reader
    println("{} {}", w.error().message(), w.error().is_timeout());
}
```

Output:

```text
write pipe: Operation timed out true
```

## See also

- [set_deadline](set_deadline.md): both directions
- [write_deadline](write_deadline.md): the deadline now
- [sgcl::net::connection](../connection.md)
