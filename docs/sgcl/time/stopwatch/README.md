[sgcl](../../README.md) › [time](../README.md)

# sgcl::time::stopwatch

```cpp
#include "sgcl/time/stopwatch.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class stopwatch;
}
```

`sgcl::time::stopwatch` is the time elapsed since a start, on [sgcl::clock](../../core/clock/README.md): the steady clock,
which a change of the system's wall clock does not move, unless a test has installed a
[manual clock](../../async/manual_clock/README.md), and then the test's time, so that code that measures itself is tested
with no real waiting. It starts when it is made, [elapsed](elapsed.md) reads it,
[restart](restart.md) reads it and starts it again, and [measure](measure.md) is the one line
for the common case: a stopwatch started, a function called and the time read.

Go keeps such a monotonic reading inside every `time.Time`, where it is invisible and lost by the first `Round(0)`
or a trip through text; here it has a name of its own, and a [datetime](../datetime/README.md) stays a plain value. What a
stopwatch gives is a [duration](../../core/duration/README.md).

## Rules

- A stopwatch is eight bytes, a point of the clock, trivially copyable: it lives anywhere.
- Nothing waits, and nothing throws but what the function given to `measure` throws.
- `measure` drops what the function returns; what it throws goes through, with no time measured.

### From code written for Go

| With Go | With sgcl::time |
|---|---|
| `start := time.Now()`, then `time.Since(start)` | `time::stopwatch sw;`, then `sw.elapsed()`, on the library's clock, a test's manual one included |
| the monotonic reading inside a `Time` | a `stopwatch`; a `datetime` holds none |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](stopwatch.md) | starts a stopwatch at once |
| [elapsed](elapsed.md) | the time since the start |
| [restart](restart.md) | the time elapsed, and a new start from now |
| [measure](measure.md) | how long a call takes (static) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::stopwatch sw;
    long sum = 0;
    for (int i : range(1000000)) {
        sum += i;
    }
    println("{} {}", sw.elapsed() < 10 * second, sum);
}
```

Output:

```text
true 499999500000
```

## See also

- [clock](../../core/clock/README.md): the clock a stopwatch reads
- [manual_clock](../../async/manual_clock/README.md): a test's clock, which moves only when the test moves it
- [duration](../../core/duration/README.md): what a stopwatch gives
- [sgcl::time](../README.md)
