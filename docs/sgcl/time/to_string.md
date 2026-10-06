[sgcl](../README.md) › [time](README.md)

# sgcl::time::to_string, sgcl::time::operator\<\< (sgcl::time::month, sgcl::time::weekday)

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    string to_string(month m) noexcept;                                                     // (1)
    string to_string(weekday d) noexcept;                                                   // (2)
    string to_string(month m, const txt::locale& l, txt::width w = txt::width::wide,        // (5)
                     txt::name_context context = txt::name_context::format);
    string to_string(weekday d, const txt::locale& l, txt::width w = txt::width::wide,      // (6)
                     txt::name_context context = txt::name_context::format);
    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,    // (3)
                                                  month m);
    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,    // (4)
                                                  weekday d);
}
```

The name of a month or of a day of the week: in English, as Go's `Month.String()` and `Weekday.String()` write it, or in
a locale.

1. `"January"` to `"December"`. A value outside 1 to 12 is `"%!Month(13)"`, its number in the parentheses, as Go
   writes it.
2. `"Monday"` to `"Sunday"`. A value outside 1 to 7 is `"%!Weekday(0)"`, the same way.
3. Writes `to_string(m)` to `os`.
4. Writes `to_string(d)` to `os`.
5. The name of the month in the locale's language (CLDR 46; declared in `sgcl/time/localized.h`), of the
   [width](../txt/width.md) `w`, in the [name_context](../txt/name_context.md) `context`: the form it takes inside a
   date ("września" in Polish), or standing alone ("wrzesień"). An empty text for a value outside 1 to 12.
6. The name of the day of the week the same way: "czwartek", "czw.", "C"; an empty text for a value outside 1 to 7.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the month |
| `d` | the day of the week |
| `os` | the stream written to |
| `l` | the locale (5–6) |
| `w` | the [width](../txt/width.md): wide, abbreviated, narrow (5–6) |
| `context` | the form inside a date or standing alone, a [name_context](../txt/name_context.md) (5–6) |

## Return value

- (1–2), (5–6) The name.
- (3–4) `os`.

## Complexity

- (1–4) Constant.
- (5–6) Logarithmic in the number of locales.

## Exceptions

- (1–2), (5–6) None.
- (3–4) What the stream throws when its exceptions are on.

## Notes

[txt::format](README.md#formatting-with-txt) writes a month and a day of the week by their names with `{}`, and
takes a pattern of their specifiers: `{:%b}` is `"Sep"`, `{:%a}` `"Thu"`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"
#include <iostream>

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24);
    println("{} {}", time::to_string(d.month()), time::to_string(d.weekday()));
    auto pl = txt::locale("pl");
    println("{} | {}", time::to_string(d.month(), pl),
            time::to_string(d.month(), pl, txt::width::wide, txt::name_context::standalone));
    println("{} | {}", time::to_string(d.weekday(), pl, txt::width::abbreviated),
            time::to_string(d.weekday(), txt::locale("ja")));
    std::cout << time::month::may << ' ' << time::weekday::friday << '\n';
    std::cout << time::to_string(time::month(13)) << ' ' << time::weekday(0) << '\n';
}
```

Output:

```text
September Thursday
września | wrzesień
czw. | 木曜日
May Friday
%!Month(13) %!Weekday(0)
```

## See also

- [month](month.md), [weekday](weekday.md): the enumerations
- [date::to_string](date/to_string.md): the text of a date
- [time](README.md)
