[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::icalendar

```cpp
#include "sgcl/encoding/icalendar.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class icalendar;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::icalendar` is one component of iCalendar ([RFC 5545](https://www.rfc-editor.org/rfc/rfc5545)) — a
VCALENDAR, or a VEVENT, VTODO, VJOURNAL, VFREEBUSY, VTIMEZONE (with its STANDARD and DAYLIGHT), VALARM or any other
inside it — with its properties ([content_line](../content_line/README.md)s) and the components inside it, in order.
Immutable, one word shared by copying. [parse](parse.md) and [load](load.md) read a calendar,
[to_string](to_string.md) and [save](save.md) write one; a calendar reads the times of its components with its own
VTIMEZONEs ([datetime_of](datetime_of.md)) and expands their recurrences ([occurrences](occurrences.md)). Go's
standard library has no iCalendar.

## Rules

- **What is read**: lines unfolded (CRLF or LF, then a space or a tab; a fold may fall inside a UTF-8 sequence),
  BEGIN and END of any component nested, every property and parameter kept as written, names in any case. Errors
  have their line and column.
- **What is refused**: an END without its BEGIN or of another component, a BEGIN without its END, a line outside the
  VCALENDAR, a top component that is no VCALENDAR, a line that is no content line (no `:`, a name that is none, a
  quote not closed), control characters, invalid UTF-8; depth and size past [icalendar::options](../icalendar-options.md).
- **Times**: UTC, a TZID of the calendar's VTIMEZONE (its STANDARD and DAYLIGHT observances expanded by their RRULEs
  and RDATEs: Outlook's `Pacific Standard Time` reads as the calendar defines it), else a TZID the system knows; a
  floating time and a DATE in the zone the program gives.
- **Occurrences**: DTSTART, its RRULEs (by [recurrence](../recurrence/README.md)'s rules), its RDATEs (DATE-TIMEs,
  DATEs and the starts of PERIODs), less its EXDATEs; a time the zone skips moves on by the skip, one it shows twice
  is the first.
- **Written**: each line folded at 75 octets and ended by CRLF.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `property` | [content_line](../content_line/README.md) |
| `options` | what a parse accepts: [icalendar::options](../icalendar-options.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](icalendar.md) | a VCALENDAR, or an empty component of a name |
| [parse, async_parse](parse.md) | a calendar of a text or a stream (static) |
| [parse_all](parse_all.md) | every calendar of a text (static) |
| [to_string](to_string.md) | the component as a file holds it |
| [load, async_load](load.md) | the calendar of a file (static) |
| [save, async_save](save.md) | the component into a file |

#### Observers

| Function | Description |
|---|---|
| [name](name.md) | VCALENDAR, VEVENT, ... |
| [properties, property_of, properties_of](properties.md) | the properties, those of a name |
| [text](text.md) | a property's text |
| [components, components_of](components.md) | the components inside, those of a name |

#### Times

| Function | Description |
|---|---|
| [datetime_of](datetime_of.md) | a property's time, with the calendar's VTIMEZONEs |
| [occurrences](occurrences.md) | the starts of a component |

#### New versions

| Function | Description |
|---|---|
| [add](add.md) | with a property or a component added |
| [set](set.md) | with a property replacing those of its name |
| [erase](erase.md) | without the properties of a name |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same name, properties and components |

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
    for (const auto& event : calendar.components_of("VEVENT")) {
        println(event.text("SUMMARY", "?"));
        for (const auto& t : calendar.occurrences(event, time::date(2026, 10, 5).at(0, 0, warsaw), time::date(2026, 10, 10).at(0, 0, warsaw))) {
            println("  {}", t.to_string());
        }
    }
}
```

Output:

```text
Stand-up, daily
  2026-10-05T09:30:00+02:00
  2026-10-06T09:30:00+02:00
  2026-10-08T09:30:00+02:00
  2026-10-09T09:30:00+02:00
```

## See also

- [content_line](../content_line/README.md), [recurrence](../recurrence/README.md), [vcard](../vcard/README.md)
- [sgcl::time](../../time/README.md)
- [sgcl::encoding](../README.md)
