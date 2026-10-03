[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::from_unix_micro

```cpp
static datetime from_unix_micro(int64_t microseconds,
                                const time::zone& z = time::zone::local()) noexcept;
```

The datetime of the microseconds since 1970-01-01T00:00:00Z (negative before 1970), in the zone `z`: the local
zone unless another is given, as in Go's `time.UnixMicro`. A count beyond the range of a datetime gives the end of
the range.

## Parameters

| Parameter | Description |
|---|---|
| `microseconds` | the microseconds since 1970 |
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
    println(time::datetime::from_unix_micro(1790246475122575, time::zone("Europe/Warsaw")));
    println(time::datetime::from_unix_micro(-1, time::zone::utc()));
}
```

Output:

```text
2026-09-24T12:41:15.122575+02:00
1969-12-31T23:59:59.999999Z
```

## See also

- [unix_micro](unix_micro.md): the microseconds back
- [from_unix](from_unix.md), [from_unix_milli](from_unix_milli.md), [from_unix_nano](from_unix_nano.md): the other
  units
- [sgcl::time::datetime](README.md)
