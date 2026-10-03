[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::from_unix_nano

```cpp
static datetime from_unix_nano(int64_t nanoseconds,
                               const time::zone& z = time::zone::local()) noexcept;
```

The datetime of the nanoseconds since 1970-01-01T00:00:00Z (negative before 1970), in the zone `z`: the local zone
unless another is given, as Go's `time.Unix(0, n)`. Every `int64_t` is an instant of the range, which is the range
of this count.

## Parameters

| Parameter | Description |
|---|---|
| `nanoseconds` | the nanoseconds since 1970 |
| `z` | the zone the datetime is seen in |

## Return value

The datetime.

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
    println(time::datetime::from_unix_nano(1790246475122575000, time::zone("Europe/Warsaw")));
    println(time::datetime::from_unix_nano(INT64_MIN, time::zone::utc()));
}
```

Output:

```text
2026-09-24T12:41:15.122575+02:00
1677-09-21T00:12:43.145224192Z
```

## See also

- [unix_nano](unix_nano.md): the nanoseconds back
- [from_unix](from_unix.md), [from_unix_milli](from_unix_milli.md), [from_unix_micro](from_unix_micro.md): the
  coarser units
- [sgcl::time::datetime](../datetime.md)
