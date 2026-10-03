[sgcl](../../README.md) › [async](../README.md) › [manual_clock](../manual_clock.md)

# sgcl::async::manual_clock::advance_to

```cpp
void advance_to(time_point t) noexcept;
```

Moves the manual time forward to the point `t`, as [advance](advance.md) moves it by a span: the tasks settled
first, the time moved and every timer due by `t` fired in the order of its deadline, the tasks those timers woke
settled at their next waits before the call returns. The clock must be installed, and `t` must not be before
[now](now.md) (assertions in debug builds): the time never goes back.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the new time, of `sgcl::clock` |

## Return value

None.

## Complexity

The timers that fall due, each fired once (a tick once per period covered), and the waits for the workers to
settle on either side of the move.

## Exceptions

None. The calls of the timer thread's `std::mutex` and `std::condition_variable` fail only on a lock the caller
holds already, which the module never takes twice.

## Notes

Called from a thread that is not a worker, as [advance](advance.md#notes) is.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();
    time_point deadline = clock.now() + 1h;
    async::event first = async::at(deadline - 1s);
    async::event second = async::at(deadline);
    clock.advance_to(deadline - 1s);
    println("{} {}", first.is_set(), second.is_set());
    clock.advance_to(deadline);
    println("{} {}", first.is_set(), second.is_set());
}
```

Output:

```text
true false
true true
```

## See also

- [advance](advance.md): by a span
- [at](../at.md), [sleep_until](../sleep_until.md): the points a test advances to
- [sgcl::async::manual_clock](../manual_clock.md)
