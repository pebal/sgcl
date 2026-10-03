[sgcl](../README.md) › [time](README.md)

# sgcl::time::now

```cpp
#include "sgcl/time/datetime.h"   // or "sgcl/time.h"

namespace sgcl::time {
    datetime now() noexcept;
}
```

The time now, read from the system's clock, in the local zone: Go's `time.Now()`. A [datetime](datetime.md), to the
nanosecond the clock gives.

While a test has a [manual_clock](../async/manual_clock.md) installed, it is the wall time of the install moved on by
as much as the manual time has been advanced, so that code which asks for the time — an expiry, a header of HTTP, a
log's rotation — is tested with no real waiting, as the timers are. The time elapsed is measured with a
[stopwatch](stopwatch.md), on the monotonic clock, not with two readings of `now()`, which a change of the system's
clock moves.

## Parameters

None.

## Return value

The current instant, in the local zone; `now().utc()` is the same instant in UTC.

## Complexity

Constant, but for the first use of the local zone in the program, which settles it once from the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    println(time::now().utc());

    async::manual_clock clock;  // a test's clock: time moves only by advance
    clock.install();
    auto start = time::now();
    clock.advance(90 * minute);
    println(time::now() - start);
}
```

Sample output:

```text
2026-10-02T09:14:27.338216Z
1h30m0s
```

## See also

- [datetime](datetime.md): what `now()` is
- [stopwatch](stopwatch.md): the time elapsed, on the monotonic clock
- [manual_clock](../async/manual_clock.md): a test's clock
- [zone::local](zone/local.md): the local zone
- [time](README.md): the module
