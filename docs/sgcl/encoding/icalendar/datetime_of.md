[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::datetime_of

```cpp
optional<time::datetime> datetime_of(const property& p) const noexcept;                                // (1)
optional<time::datetime> datetime_of(const property& p, const time::zone& floating) const noexcept;    // (2)
```

On a calendar, the instant of a DATE-TIME of one of its components: UTC; a TZID of one of the calendar's VTIMEZONEs,
in the fixed zone of its offset then; else a TZID the system knows, in that zone. A floating time, and a DATE at its
midnight, as that clock in a zone:

1. The system's local zone.
2. The zone given.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the property (DTSTART, DUE, RECURRENCE-ID) |
| `floating` | the zone of a floating time's clock |

## Return value

The instant, or `nullopt` for a value that is no DATE or DATE-TIME, an unknown TZID, a year outside [time::datetime](../../time/datetime/README.md)'s.

## Complexity

Linear in the calendar's components, and in a VTIMEZONE's observances.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto calendar = encoding::icalendar::parse(
        "BEGIN:VCALENDAR\r\nBEGIN:VTIMEZONE\r\nTZID:Pacific Standard Time\r\n"
        "BEGIN:STANDARD\r\nDTSTART:16011104T020000\r\nRRULE:FREQ=YEARLY;BYDAY=1SU;BYMONTH=11\r\nTZOFFSETFROM:-0700\r\nTZOFFSETTO:-0800\r\nEND:STANDARD\r\n"
        "BEGIN:DAYLIGHT\r\nDTSTART:16010311T020000\r\nRRULE:FREQ=YEARLY;BYDAY=2SU;BYMONTH=3\r\nTZOFFSETFROM:-0800\r\nTZOFFSETTO:-0700\r\nEND:DAYLIGHT\r\n"
        "END:VTIMEZONE\r\nEND:VCALENDAR\r\n").value();
    encoding::content_line start("DTSTART", {{"TZID", {"Pacific Standard Time"}}}, "20261006T090000");
    println(calendar.datetime_of(start)->to_string());
    println(calendar.datetime_of(encoding::content_line("DTSTART", "20261006T090000"), time::zone::utc())->to_string());
}
```

Output:

```text
2026-10-06T09:00:00-07:00
2026-10-06T09:00:00Z
```

## See also

- [occurrences](occurrences.md)
- [content_line::as_datetime](../content_line/as_date.md)
- [sgcl::encoding::icalendar](README.md)
