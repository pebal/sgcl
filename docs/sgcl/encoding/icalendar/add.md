[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::add

```cpp
icalendar add(const property& p) const noexcept;             // (1)
icalendar add(const icalendar& component) const noexcept;    // (2)
```

New components; the component itself never changes.

1. With the property added after the others.
2. With the component added after the others.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the property |
| `component` | the component |

## Return value

The new component.

## Complexity

Linear in the properties, or in the components.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::icalendar alarm = encoding::icalendar("VALARM").add(encoding::content_line("ACTION", "DISPLAY")).add(encoding::content_line("TRIGGER", "-PT10M"));
    encoding::icalendar event = encoding::icalendar("VEVENT").add(encoding::content_line("UID", "1")).add(alarm);
    print(event.to_string());
}
```

Output:

```text
BEGIN:VEVENT
UID:1
BEGIN:VALARM
ACTION:DISPLAY
TRIGGER:-PT10M
END:VALARM
END:VEVENT
```

## See also

- [set](set.md)
- [sgcl::encoding::icalendar](README.md)
