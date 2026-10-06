[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::local_time, local_datetime

```cpp
static toml local_time(duration since_midnight);                             // (1)
static toml local_datetime(const time::date& d, duration since_midnight);    // (2)
```

A time of the clock without a zone: what TOML calls a local time and a local date-time, for which
[sgcl::time](../../time/README.md) has no type of its own.

1. A local time, `07:32:00`, the fraction of a second to its last digit that is not 0.
2. A local date-time, `1979-05-27T07:32:00`.

## Parameters

| Parameter | Description |
|---|---|
| `since_midnight` | the time of the clock, 0 to less than a day |
| `d` | the date |

## Return value

The value.

## Complexity

Constant.

## Exceptions

`invalid_argument` for a time below 0 or of a day or more.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml v = encoding::toml::table({
        {"alarm", encoding::toml::local_time(7 * hour + 30 * minute)},
        {"meeting", encoding::toml::local_datetime(time::date(2026, 10, 6), 14 * hour + 500 * millisecond)}});
    print(v.to_string());
}
```

Output:

```text
alarm = 07:30:00
meeting = 2026-10-06T14:00:00.5
```

## See also

- [as_time](as_time.md)
- [as_datetime](as_datetime.md)
- [sgcl::encoding::toml](README.md)
