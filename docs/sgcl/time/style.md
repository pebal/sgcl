[sgcl](../README.md) › [time](README.md)

# sgcl::time::style

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    enum class style : uint8_t {
        none,
        brief,
        medium,
        detailed,
        full,
    };
}
```

The length of a locale's pattern of a date or a time: CLDR's four lengths (short, medium, long and full, named here
`brief`, `medium`, `detailed` and `full`), and none, which leaves the date or the time out. A
[datetime](datetime/format.md), a [date](date/format.md) and a [date_format](date_format/README.md) take one for the
date and one for the time; a date and a time together are joined by the pattern of the date's length.

| Value | Description |
|---|---|
| `none` | no date, or no time |
| `brief` | CLDR's short: `24.09.2026`, `14:05`; `9/24/26`, `2:05 PM` |
| `medium` | CLDR's medium: `24 wrz 2026`, `14:05:09`; `Sep 24, 2026`, `2:05:09 PM` |
| `detailed` | CLDR's long: `24 września 2026`, `14:05:09 CEST` (the zone's name with the [display names](../txt/names.md), `GMT+2` without); `September 24, 2026` |
| `full` | CLDR's full: `czwartek, 24 września 2026`; `Thursday, September 24, 2026` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto t = time::date(2026, 9, 24).at(14, 5, time::zone::utc());
    for (auto s :
         {time::style::brief, time::style::medium, time::style::detailed, time::style::full}) {
        println("{} | {}", t.format(txt::locale("pl"), s, time::style::none),
                t.format(txt::locale("en"), s, time::style::none));
    }
}
```

Output:

```text
24.09.2026 | 9/24/26
24 wrz 2026 | Sep 24, 2026
24 września 2026 | September 24, 2026
czwartek, 24 września 2026 | Thursday, September 24, 2026
```

## See also

- [datetime::format](datetime/format.md)
- [date_format](date_format/README.md)
- [sgcl::time](README.md)
