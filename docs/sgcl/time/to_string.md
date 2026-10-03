[sgcl](../README.md) › [time](README.md)

# sgcl::time::to_string, sgcl::time::operator\<\< (sgcl::time::month, sgcl::time::weekday)

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    /*(1)*/ string to_string(month m) noexcept;
    /*(2)*/ string to_string(weekday d) noexcept;
    /*(3)*/ template<class CharT, class Traits>
            std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                                          month m);
    /*(4)*/ template<class CharT, class Traits>
            std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                                          weekday d);
}
```

The English name of a month or of a day of the week, as Go's `Month.String()` and `Weekday.String()` write it.

1. `"January"` to `"December"`. A value outside 1 to 12 is `"%!Month(13)"`, its number in the parentheses, as Go
   writes it.
2. `"Monday"` to `"Sunday"`. A value outside 1 to 7 is `"%!Weekday(0)"`, the same way.
3. Writes `to_string(m)` to `os`.
4. Writes `to_string(d)` to `os`.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the month |
| `d` | the day of the week |
| `os` | the stream written to |

## Return value

- (1–2) The name.
- (3–4) `os`.

## Complexity

Constant.

## Exceptions

- (1–2) None.
- (3–4) What the stream throws when its exceptions are on.

## Notes

[txt::format](README.md#formatting-with-txt) writes a month and a day of the week by their names with `{}`, and
takes a pattern of their specifiers: `{:%b}` is `"Sep"`, `{:%a}` `"Thu"`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <iostream>

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24);
    println("{} {}", time::to_string(d.month()), time::to_string(d.weekday()));
    std::cout << time::month::may << ' ' << time::weekday::friday << '\n';
    std::cout << time::to_string(time::month(13)) << ' ' << time::weekday(0) << '\n';
}
```

Output:

```text
September Thursday
May Friday
%!Month(13) %!Weekday(0)
```

## See also

- [month](month.md), [weekday](weekday.md): the enumerations
- [date::to_string](date/to_string.md): the text of a date
- [time](README.md)
