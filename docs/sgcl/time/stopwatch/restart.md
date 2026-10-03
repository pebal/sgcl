[sgcl](../../README.md) › [time](../README.md) › [stopwatch](../stopwatch.md)

# sgcl::time::stopwatch::restart

```cpp
duration restart() noexcept;
```

Reads the time elapsed and starts the stopwatch again from now, with one read of the clock: the laps of a loop
add up to the whole, with nothing lost between them.

## Parameters

None.

## Return value

What had elapsed since the construction or the last restart.

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
    clock.advance(1500ms);
    println("{} {}", sw.restart(), sw.elapsed());
    clock.advance(250ms);
    println("{}", sw.restart());
}
```

Output:

```text
1.5s 0s
250ms
```

## See also

- [elapsed](elapsed.md): the time since the start, the stopwatch running on
- [sgcl::time::stopwatch](../stopwatch.md)
