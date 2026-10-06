[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::as_date, as_datetime

```cpp
optional<time::date> as_date() const noexcept;                                      // (1)
optional<time::datetime> as_datetime() const noexcept;                              // (2)
optional<time::datetime> as_datetime(const time::zone& floating) const noexcept;    // (3)
```

1. A DATE (`19970714`), or the date of a DATE-TIME as written.
2. A DATE-TIME in UTC (`19970714T173000Z`), or of a TZID the system knows (an IANA name: `Europe/Warsaw`), as that
   clock in the zone (a time the zone skips moved on by the skip, one it shows twice the first: RFC 5545 §3.3.5);
   `nullopt` for a floating one, a DATE, and a TZID of a calendar's own VTIMEZONE, which
   [icalendar::datetime_of](../icalendar/datetime_of.md) reads.
3. The same, a floating DATE-TIME as that clock in the zone given.

A time outside [time::datetime](../../time/datetime/README.md)'s years 1677 to 2262 is `nullopt`.

## Parameters

| Parameter | Description |
|---|---|
| `floating` | the zone of a floating time's clock |

## Return value

The value, or `nullopt`.

## Complexity

Constant; a TZID as the zone's loading.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::content_line start("DTSTART", {{"TZID", {"America/New_York"}}}, "19970902T090000");
    println("{} {}", start.as_date(), start.as_datetime()->to_string());
    encoding::content_line floating("DTSTART", "19970902T090000");
    println("{} {}", floating.as_datetime(), floating.as_datetime(time::zone::utc())->to_string());
}
```

Output:

```text
1997-09-02 1997-09-02T09:00:00-04:00
nullopt 1997-09-02T09:00:00Z
```

## See also

- [icalendar::datetime_of](../icalendar/datetime_of.md)
- [sgcl::encoding::content_line](README.md)
