[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::as_datetime

```cpp
optional<time::datetime> as_datetime() const noexcept;                       // (1)
optional<time::datetime> as_datetime(const time::zone& z) const noexcept;    // (2)
```

The instant of a date-time as a [datetime](../../time/datetime/README.md), whose range is the years 1677 to 2262: a
date-time outside it gives `nullopt` ([as_date](as_date.md) and [as_time](as_time.md) read it all the
same). A leap second, `23:59:60`, is the second before it.

1. Of an offset date-time, in the fixed zone of its offset; `nullopt` for every other value.
2. Of a local date-time, or of a local date at midnight, as that clock in the zone `z` (a time the zone's clock skips
   or shows twice as [date::at](../../time/date/at.md) takes it); of an offset date-time, its instant seen in `z`;
   `nullopt` for every other value.

## Parameters

| Parameter | Description |
|---|---|
| `z` | the zone of a local value's clock |

## Return value

The instant, or `nullopt`.

## Complexity

Constant; (2) as the zone's lookup.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("a = 1979-05-27T07:32:00-08:00\nb = 1979-05-27T07:32:00\nc = 9999-12-31T00:00:00Z").value();
    println(v["a"].as_datetime()->to_string());
    println(v["a"].as_datetime(time::zone::utc())->to_string());
    println(v["b"].as_datetime());
    println(v["b"].as_datetime(time::zone::fixed(2 * hour))->to_string());
    println(v["c"].as_datetime());
}
```

Output:

```text
1979-05-27T07:32:00-08:00
1979-05-27T15:32:00Z
nullopt
1979-05-27T07:32:00+02:00
nullopt
```

## See also

- [as_date](as_date.md)
- [as_time](as_time.md)
- [sgcl::encoding::toml](README.md)
