[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::name

```cpp
string name() const noexcept;
```

The component's name, upper-cased: VCALENDAR, VEVENT, VALARM, X-ANYTHING.

## Parameters

None.

## Return value

The name.

## Complexity

Constant.

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
    for (const auto& c : calendar.components()) {
        println(c.name());
    }
}
```

Output:

```text
VEVENT
VTODO
```

## See also

- [components](components.md)
- [sgcl::encoding::icalendar](README.md)
