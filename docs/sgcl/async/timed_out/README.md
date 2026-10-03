[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::timed_out

```cpp
#include "sgcl/async/timeout.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class timed_out;
}
```

The error of a race lost to the time: what [with_timeout](../with_timeout.md) and [with_deadline](../with_deadline.md)
by a point give in their `expected<T, timed_out>` when the deadline came before the task's result. A value, not
an exception: a deadline that passed is a failure the library reports, and every failure of the library is an
[expected](../../core/expected/README.md). It carries nothing but its kind: `message()` says what it is, and every
`timed_out` equals every other. Go's counterpart is `context.DeadlineExceeded`.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the implicit ones: a `timed_out`, copied freely |
| [message](message.md) | the text of the error, `"timed out"` |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares two errors: always equal |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> slow() {
    co_await async::sleep(1h);
    co_return 42;
}

int main() {
    async::manual_clock clock;
    clock.install();
    auto r = async::spawn(async::with_timeout(slow(), 1s));
    clock.advance(1s);
    expected<int, async::timed_out> result = r.wait();
    println("{}", result.has_value());
    println("{}", result.error().message());
    println("{}", result.error() == async::timed_out());
}
```

Output:

```text
false
timed out
true
```

## See also

- [stopped](../stopped/README.md): the error of a race lost to a token's stop
- [with_timeout](../with_timeout.md), [with_deadline](../with_deadline.md): the races that give it
- [expected](../../core/expected/README.md): where it stands
