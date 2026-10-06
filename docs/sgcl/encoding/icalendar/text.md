[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::text

```cpp
string text(const string& name, const string& fallback) const noexcept;
```

The first property of the name read as [TEXT](../content_line/text.md) (SUMMARY, LOCATION, DESCRIPTION), or
`fallback` when there is none.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the property's name |
| `fallback` | what is given when there is none |

## Return value

The text.

## Complexity

Linear in the properties.

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
    println("{} | {}", event.text("SUMMARY", "?"), event.text("LOCATION", "nowhere"));
}
```

Output:

```text
Stand-up, daily | nowhere
```

## See also

- [properties](properties.md)
- [sgcl::encoding::icalendar](README.md)
