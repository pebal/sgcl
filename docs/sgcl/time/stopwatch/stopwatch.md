[sgcl](../../README.md) › [time](../README.md) › [stopwatch](README.md)

# sgcl::time::stopwatch::stopwatch

```cpp
stopwatch() noexcept;
```

A stopwatch started at once: its start is [clock::now()](../../core/clock/README.md), the steady clock's time, or a test's
[manual clock's](../../async/manual_clock/README.md) while one is installed.

## Parameters

None.

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
    async::manual_clock clock;  // a test's clock: time moves only by advance
    clock.install();
    time::stopwatch sw;
    println("{}", sw.elapsed());
    clock.advance(1500ms);
    println("{}", sw.elapsed());
}
```

Output:

```text
0s
1.5s
```

## See also

- [elapsed](elapsed.md): the time since the start
- [restart](restart.md): a new start
- [sgcl::time::stopwatch](README.md)
