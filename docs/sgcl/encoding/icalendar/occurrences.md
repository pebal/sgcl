[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::occurrences

```cpp
vector<time::datetime> occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to) const;                  // (1)
vector<time::datetime> occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to, size_t limit) const;    // (2)
vector<time::datetime> occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to, size_t limit,
                                   const time::zone& floating) const;                                                                        // (3)
```

On a calendar, the starts of one of its components, by the [rules](README.md#rules): DTSTART, the times of its
RRULEs and RDATEs, less its EXDATEs; those at or after `from` and before `to`, in order, in DTSTART's zone (a
VTIMEZONE's offset as a fixed zone). Empty without a DTSTART or with an unknown TZID.

1. At most 100 000, floating times and DATEs in the system's local zone.
2. At most `limit`.
3. Floating times and DATEs (an all-day event at midnight) in the zone given.

## Parameters

| Parameter | Description |
|---|---|
| `component` | a VEVENT, a VTODO, a VJOURNAL of the calendar |
| `from`, `to` | the range, `to` not in it |
| `limit` | the most instances |
| `floating` | the zone of floating times and DATEs |

## Return value

The starts.

## Complexity

As [recurrence::occurrences](../recurrence/occurrences.md) for each RRULE, and linear in the RDATEs and EXDATEs.

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
        "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//example//team//EN\r\n"
        "BEGIN:VEVENT\r\nUID:standup@example.com\r\nSUMMARY:Stand-up\\, daily\r\n"
        "DTSTART;TZID=Europe/Warsaw:20261005T093000\r\nRRULE:FREQ=WEEKLY;BYDAY=MO,TU,WE,TH,FR\r\n"
        "EXDATE;TZID=Europe/Warsaw:20261007T093000\r\nEND:VEVENT\r\n"
        "BEGIN:VTODO\r\nUID:report@example.com\r\nSUMMARY:Report\r\nDUE:20261009T170000Z\r\nEND:VTODO\r\n"
        "END:VCALENDAR\r\n").value();
    auto warsaw = time::zone::load("Europe/Warsaw").value();
    auto event = calendar.components_of("VEVENT")[0];
    for (const auto& t : calendar.occurrences(event, time::date(2026, 10, 1).at(0, 0, warsaw), time::date(2026, 10, 13).at(0, 0, warsaw), 6)) {
        println(t.to_string());
    }
}
```

Output:

```text
2026-10-05T09:30:00+02:00
2026-10-06T09:30:00+02:00
2026-10-08T09:30:00+02:00
2026-10-09T09:30:00+02:00
2026-10-12T09:30:00+02:00
```

## See also

- [recurrence](../recurrence/README.md)
- [datetime_of](datetime_of.md)
- [sgcl::encoding::icalendar](README.md)
