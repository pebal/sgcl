[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::to_sys

```cpp
std::chrono::sys_time<std::chrono::nanoseconds> to_sys() const noexcept;
```

The instant as a point of `std::chrono::system_clock` in nanoseconds, for code written with `<chrono>`: the same
count, the zone left behind. The [constructor](datetime.md) takes such a point back, with a zone.

## Parameters

None.

## Return value

The `std::chrono::sys_time<std::chrono::nanoseconds>` of the instant.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <chrono>

using namespace sgcl;

int main() {
    auto t = time::date(2026, 9, 24).at(12, 41, 15, time::zone("Europe/Warsaw"));
    std::chrono::sys_time<std::chrono::nanoseconds> sys = t.to_sys();
    println(std::chrono::floor<std::chrono::seconds>(sys));
    println(time::datetime(sys, time::zone::utc()));
}
```

Output:

```text
2026-09-24 10:41:15
2026-09-24T10:41:15Z
```

## See also

- [(constructor)](datetime.md): a datetime of a `std::chrono::sys_time`
- [unix_nano](unix_nano.md): the count as a number
- [sgcl::time::datetime](../datetime.md)
