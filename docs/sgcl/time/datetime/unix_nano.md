[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::unix_nano

```cpp
int64_t unix_nano() const noexcept;
```

The nanoseconds since 1970-01-01T00:00:00Z: Go's `t.UnixNano()`, and the count a datetime holds, so every instant
of its range has one.

## Parameters

None.

## Return value

The nanoseconds since 1970, negative before it.

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
    println((t + 122575 * microsecond).unix_nano());
    println(time::datetime().unix_nano());
}
```

Output:

```text
1790246475122575000
0
```

## See also

- [from_unix_nano](from_unix_nano.md): the datetime of the nanoseconds
- [to_sys](to_sys.md): the same count as a `std::chrono::sys_time`
- [unix](unix.md), [unix_milli](unix_milli.md), [unix_micro](unix_micro.md): the coarser units
- [sgcl::time::datetime](README.md)
