[sgcl](../../README.md) › [time](../README.md) › [stopwatch](README.md)

# sgcl::time::stopwatch::measure

```cpp
template<class F>
    requires std::invocable<F&>
static duration measure(F&& f) noexcept(std::is_nothrow_invocable_v<F&>);
```

How long `f()` takes, on the stopwatch's [clock](../../core/clock/README.md): a stopwatch started, `f` called and the time
read, in one line — `duration d = time::stopwatch::measure([&] { build_index(); });`. What `f` returns is dropped.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function to call, with no arguments |

## Return value

The time the call took.

## Complexity

One call of `f` and two reads of the clock.

## Exceptions

What `f` throws, which goes through with no time measured; none when `f` is nothrow-invocable.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();
    println("{}", time::stopwatch::measure([&] { clock.advance(250ms); }));
    println("{}", noexcept(time::stopwatch::measure([]() noexcept {})));
}
```

Output:

```text
250ms
true
```

## See also

- [(constructor)](stopwatch.md), [elapsed](elapsed.md): the same by hand
- [sgcl::time::stopwatch](README.md)
