[sgcl](../README.md) › [time](README.md)

# sgcl::time::month

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    enum class month : uint8_t {
        january = 1,
        february,
        march,
        april,
        may,
        june,
        july,
        august,
        september,
        october,
        november,
        december,
    };
}
```

A month of the year, January 1 to December 12, as Go's `time.Month`: a name for the number, and `int(m)` the
number itself. A [date](date/README.md) answers with one ([month](date/month.md)) and is made with either a number or a
name, `date(2026, 9, 25)` or `date(2026, time::month::september, 25)`.

Its text is the English name, as Go's `Month.String()` writes it: [to_string](to_string.md) and `operator<<` write
`"September"`. In [txt::format](README.md#formatting-with-txt) `{}` writes the name too, and a pattern takes `%b`
and `%h` (`"Sep"`), `%B` (`"September"`) and `%m` (`"09"`); another specifier does not compile. A value outside 1
to 12 is written `"%!Month(13)"`, in whatever pattern, as Go writes it.

| Value | Description |
|---|---|
| `january` | 1 |
| `february` | 2 |
| `march` | 3 |
| `april` | 4 |
| `may` | 5 |
| `june` | 6 |
| `july` | 7 |
| `august` | 8 |
| `september` | 9 |
| `october` | 10 |
| `november` | 11 |
| `december` | 12 |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::month m = time::date(2026, 9, 24).month();
    println("{} {} {}", m, int(m), m == time::month::september);
    println("[{:%b}] [{:%m}] [{:>12}]", m, m, m);
    println("{}", time::date(2026, time::month::february, 30));
    println(time::month(13));
}
```

Output:

```text
September 9 true
[Sep] [09] [   September]
2026-03-02
%!Month(13)
```

## See also

- [to_string, operator\<\<](to_string.md): the name
- [date::month](date/month.md): the month of a date
- [weekday](weekday.md): the day of the week
- [time](README.md)
