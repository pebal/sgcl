[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::set

```cpp
icalendar set(const property& p) const noexcept;
```

A new component with the property in place of every property of its name: where the first of them was, at the end
when there was none.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the property |

## Return value

The new component.

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
    encoding::icalendar calendar;
    print(calendar.set(encoding::content_line("PRODID", "-//example//app//EN")).to_string());
}
```

Output:

```text
BEGIN:VCALENDAR
VERSION:2.0
PRODID:-//example//app//EN
END:VCALENDAR
```

## See also

- [add](add.md)
- [erase](erase.md)
- [sgcl::encoding::icalendar](README.md)
