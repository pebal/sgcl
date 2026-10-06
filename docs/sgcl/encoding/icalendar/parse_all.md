[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::parse_all

```cpp
static expected<vector<icalendar>, error> parse_all(const string& text) noexcept;                      // (1)
static expected<vector<icalendar>, error> parse_all(const string& text, const options& o) noexcept;    // (2)
```

Every VCALENDAR of the text, in order; none for an empty text.

1. With the default [options](../icalendar-options.md).
2. With the options given.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text |
| `o` | what is accepted |

## Return value

The calendars, or the [error](../error/README.md) of the first that is not one, as [parse](parse.md) gives it.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto all = encoding::icalendar::parse_all("BEGIN:VCALENDAR\nX-WR-CALNAME:Home\nEND:VCALENDAR\nBEGIN:VCALENDAR\nX-WR-CALNAME:Work\nEND:VCALENDAR\n");
    for (const auto& c : all.value()) {
        println(c.text("X-WR-CALNAME", "?"));
    }
}
```

Output:

```text
Home
Work
```

## See also

- [parse](parse.md)
- [sgcl::encoding::icalendar](README.md)
