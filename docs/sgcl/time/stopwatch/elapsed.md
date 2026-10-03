[sgcl](../../README.md) › [time](../README.md) › [stopwatch](../stopwatch.md)

# sgcl::time::stopwatch::elapsed

```cpp
duration elapsed() const noexcept;
```

The time since the start, on the [clock](../../core/clock.md) the stopwatch reads; the stopwatch runs on.

## Parameters

None.

## Return value

The time elapsed since the construction or the last [restart](restart.md).

## Complexity

Constant: one read of the clock.

## Exceptions

None.

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
    time::stopwatch sw;
    clock.advance(250ms);
    println("{}", sw.elapsed());
    clock.advance(2s);
    println("{}", sw.elapsed());
}
```

Output:

```text
250ms
2.25s
```

## See also

- [restart](restart.md): the time elapsed, and a new start
- [measure](measure.md): how long a call takes
- [sgcl::time::stopwatch](../stopwatch.md)
