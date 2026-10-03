[sgcl](../README.md) › [time](README.md)

# sgcl::time::weekday

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    enum class weekday : uint8_t {
        monday = 1,
        tuesday,
        wednesday,
        thursday,
        friday,
        saturday,
        sunday,
    };
}
```

A day of the week, numbered as ISO 8601 numbers it: Monday 1 to Sunday 7, not C's and Go's Sunday 0 (it is
`std::chrono::weekday`'s `iso_encoding()`, not its `c_encoding()`). `int(d)` is the number. A [date](date.md) and a
[datetime](datetime.md) answer with one ([weekday](date/weekday.md)).

Its text is the English name, as Go's `Weekday.String()` writes it: [to_string](to_string.md) and `operator<<`
write `"Monday"`. In [txt::format](README.md#formatting-with-txt) `{}` writes the name too, and a pattern takes `%a`
(`"Mon"`), `%A` (`"Monday"`), `%u` (Monday 1 to Sunday 7) and `%w` (Sunday 0 to Saturday 6); another specifier
does not compile. A value outside 1 to 7 is written `"%!Weekday(0)"`, in whatever pattern, as Go writes it.

| Value | Description |
|---|---|
| `monday` | 1 |
| `tuesday` | 2 |
| `wednesday` | 3 |
| `thursday` | 4 |
| `friday` | 5 |
| `saturday` | 6 |
| `sunday` | 7 |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::weekday d = time::date(2026, 9, 27).weekday();
    println("{} {} {}", d, int(d), d == time::weekday::sunday);
    println("[{:%a}] [{:%u}] [{:%w}] [{:<8}]", d, d, d, d);
    println(time::weekday(0));
}
```

Output:

```text
Sunday 7 true
[Sun] [7] [0] [Sunday  ]
%!Weekday(0)
```

## See also

- [to_string, operator\<\<](to_string.md): the name
- [date::weekday](date/weekday.md): the day of the week of a date
- [iso_week](iso_week.md): the week of ISO 8601
- [month](month.md): the month
- [time](README.md)
