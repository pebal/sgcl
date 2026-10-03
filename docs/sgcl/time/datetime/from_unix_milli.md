[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::from_unix_milli

```cpp
static datetime from_unix_milli(int64_t milliseconds,
                                const time::zone& z = time::zone::local()) noexcept;
```

The datetime of the milliseconds since 1970-01-01T00:00:00Z (negative before 1970), in the zone `z`: the local
zone unless another is given, as in Go's `time.UnixMilli`. A count beyond the range of a datetime gives the end of
the range. The unit is written out in the name, as Go's `UnixMilli` and [duration](../../core/duration/README.md)'s methods
have it.

## Parameters

| Parameter | Description |
|---|---|
| `milliseconds` | the milliseconds since 1970 |
| `z` | the zone the datetime is seen in |

## Return value

The datetime, saturated at the ends of the range.

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
    println(time::datetime::from_unix_milli(1790246475122, time::zone("Europe/Warsaw")));
    println(time::datetime::from_unix_milli(-500, time::zone::utc()));
}
```

Output:

```text
2026-09-24T12:41:15.122+02:00
1969-12-31T23:59:59.5Z
```

## See also

- [unix_milli](unix_milli.md): the milliseconds back
- [from_unix](from_unix.md), [from_unix_micro](from_unix_micro.md), [from_unix_nano](from_unix_nano.md): the other
  units
- [sgcl::time::datetime](README.md)
