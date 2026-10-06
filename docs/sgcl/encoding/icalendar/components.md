[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::components, components_of

```cpp
slice<const icalendar> components() const noexcept;                    // (1)
vector<icalendar> components_of(const string& name) const noexcept;    // (2)
```

1. The components inside, in order.
2. Those of the name, in any case: a calendar's events are `components_of("VEVENT")`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the components' name |

## Return value

The components.

## Complexity

(1) Constant; (2) linear in the components.

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
    println("{} {} {}", calendar.components().size(), calendar.components_of("vevent").size(), calendar.components_of("VJOURNAL").size());
}
```

Output:

```text
2 1 0
```

## See also

- [name](name.md)
- [sgcl::encoding::icalendar](README.md)
