[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::from_unix

```cpp
static datetime from_unix(int64_t seconds, const time::zone& z = time::zone::local()) noexcept;
```

The datetime of the seconds since 1970-01-01T00:00:00Z, Unix time (negative before 1970), in the zone `z`: the
local zone unless another is given, as in Go's `time.Unix`. A count beyond the range of a datetime, the years 1677
to 2262, gives the end of the range.

## Parameters

| Parameter | Description |
|---|---|
| `seconds` | the seconds since 1970 |
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
    println(time::datetime::from_unix(1790246475, time::zone("Europe/Warsaw")));
    println(time::datetime::from_unix(-1, time::zone::utc()));
    println(time::datetime::from_unix(INT64_MAX, time::zone::utc()));
}
```

Output:

```text
2026-09-24T12:41:15+02:00
1969-12-31T23:59:59Z
2262-04-11T23:47:16.854775807Z
```

## See also

- [from_unix_milli](from_unix_milli.md), [from_unix_micro](from_unix_micro.md), [from_unix_nano](from_unix_nano.md):
  the finer units
- [unix](unix.md): the seconds back
- [sgcl::time::datetime](../datetime.md)
