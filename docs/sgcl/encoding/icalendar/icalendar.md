[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::icalendar

```cpp
icalendar() noexcept;                               // (1)
explicit icalendar(const string& name) noexcept;    // (2)
```

1. A VCALENDAR of `VERSION:2.0` and `PRODID:-//sgcl//sgcl//EN`, nothing else: the start of a calendar a program
   writes.
2. An empty component of the name, upper-cased: `icalendar("VEVENT")`.

The copy and the move are the implicit ones and copy the handle.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the component's name |

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
    encoding::icalendar calendar = encoding::icalendar().add(
        encoding::icalendar("VEVENT")
            .add(encoding::content_line("UID", "lunch@example.com"))
            .add(encoding::content_line::text("SUMMARY", "Lunch, with the team"))
            .add(encoding::content_line("DTSTART", {{"TZID", {"Europe/Warsaw"}}}, "20261006T120000")));
    print(calendar.to_string());
}
```

Output:

```text
BEGIN:VCALENDAR
VERSION:2.0
PRODID:-//sgcl//sgcl//EN
BEGIN:VEVENT
UID:lunch@example.com
SUMMARY:Lunch\, with the team
DTSTART;TZID=Europe/Warsaw:20261006T120000
END:VEVENT
END:VCALENDAR
```

## See also

- [add](add.md)
- [sgcl::encoding::icalendar](README.md)
