[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::unix

```cpp
int64_t unix() const noexcept;
```

The seconds since 1970-01-01T00:00:00Z, Unix time: Go's `t.Unix()`. The division rounds down, toward the past:
half a second before 1970, 1969-12-31T23:59:59.5Z, is `-1`, and its [nanosecond](nanosecond.md) is `500000000`.
The zone does not count: the same instant has the same Unix time in every zone.

## Parameters

None.

## Return value

The whole seconds since 1970, negative before it, rounded down.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto t = time::date(2026, 9, 24).at(12, 41, 15, time::zone("Europe/Warsaw"));
    println("{} {}", t.unix(), t.utc().unix());
    auto half = time::datetime::from_unix_milli(-500, time::zone::utc());
    println("{} {} {}", half, half.unix(), half.nanosecond());
}
```

Output:

```text
1790246475 1790246475
1969-12-31T23:59:59.5Z -1 500000000
```

## See also

- [from_unix](from_unix.md): the datetime of the seconds
- [unix_milli](unix_milli.md), [unix_micro](unix_micro.md), [unix_nano](unix_nano.md): the finer units
- [sgcl::time::datetime](README.md)
