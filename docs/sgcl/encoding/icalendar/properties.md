[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::properties, property_of, properties_of

```cpp
slice<const property> properties() const noexcept;                    // (1)
optional<property> property_of(const string& name) const noexcept;    // (2)
vector<property> properties_of(const string& name) const noexcept;    // (3)
```

1. The properties in order.
2. The first property of the name, in any case; `nullopt` when there is none.
3. Every property of the name, in order: ATTENDEE, EXDATE, CATEGORIES may be given several times.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the property's name |

## Return value

The properties, or the first.

## Complexity

(1) Constant; (2–3) linear in the properties.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
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
    auto event = calendar.components_of("VEVENT")[0];
    println(event.properties().size());
    println(event.property_of("DTSTART")->param("TZID", "?"));
    println(event.properties_of("ATTENDEE").size());
}
```

Output:

```text
5
Europe/Warsaw
0
```

## See also

- [text](text.md)
- [sgcl::encoding::icalendar](README.md)
