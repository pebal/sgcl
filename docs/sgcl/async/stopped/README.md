[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::stopped

```cpp
#include "sgcl/async/timeout.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class stopped;
}
```

The error of a race lost to a token's stop: what [with_deadline](../with_deadline.md)`(t, token)` gives in its
`expected<T, stopped>` when the stop came before the task's result. The stop may be a deadline given to the
source ([stop_after](../stop_source/stop_after.md), [stop_at](../stop_source/stop_at.md)) or a stop by hand: the
library cannot tell which, where Go tells `context.DeadlineExceeded` from `context.Canceled`. A value, not an
exception, as every failure of the library is an [expected](../../core/expected/README.md); it carries nothing but its kind:
`message()` says what it is, and every `stopped` equals every other.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the implicit ones: a `stopped`, copied freely |
| [message](message.md) | the text of the error, `"stopped"` |

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
    async::stop_source source;
    auto r = async::spawn(async::with_deadline(slow(), source.token()));
    source.request_stop();
    expected<int, async::stopped> result = r.wait();
    println("{}", result.has_value());
    println("{}", result.error().message());
    println("{}", result.error() == async::stopped());
}
```

Output:

```text
false
stopped
true
```

## See also

- [timed_out](../timed_out/README.md): the error of a race lost to the time
- [with_deadline](../with_deadline.md): the race that gives it
- [stop_source](../stop_source/README.md): the stop
