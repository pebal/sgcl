[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::time_unit

```cpp
#include "sgcl/txt/relative_time.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class time_unit : uint8_t {
        second,
        minute,
        hour,
        day,
        week,
        month,
        quarter,
        year,
    };
}
```

The unit of a time relative to now ([format_relative](format_relative.md)): CLDR's relative fields.

| Value | Description |
|---|---|
| `second` | `in 5 seconds`; 0 is `now` |
| `minute` | `5 minutes ago`; 0 is `this minute` |
| `hour` | `in 2 hours`; 0 is `this hour` |
| `day` | `in 3 days`; -1, 0 and 1 are `yesterday`, `today`, `tomorrow`, and in some languages ±2 a word too |
| `week` | `2 weeks ago`; `last week`, `this week`, `next week` |
| `month` | `in 3 months`; `last month`... |
| `quarter` | `in 1 quarter`; `last quarter`... |
| `year` | `10 years ago`; `last year`... |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto u : {txt::time_unit::second, txt::time_unit::day, txt::time_unit::year}) {
        println("{} | {}", txt::format_relative(0, u, txt::locale("en")),
                txt::format_relative(-1, u, txt::locale("en")));
    }
}
```

Output:

```text
now | 1 second ago
today | yesterday
this year | last year
```

## See also

- [format_relative](format_relative.md)
- [sgcl::txt](README.md)
