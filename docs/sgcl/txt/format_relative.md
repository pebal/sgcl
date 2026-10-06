[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_relative

```cpp
string format_relative(double value, time_unit unit, const locale& l = {},    // (1)
                       const relative_options& o = {});
string format_relative(const duration& d, const locale& l = {},               // (2)
                       const relative_options& o = {});
```

Returns a time relative to now as a locale says it (CLDR's relative time fields, LDML Part 4): "za 3 dni", "3 hours
ago", "yesterday", "now". A negative value is the past, a positive one (and 0) the future; a word the language has
stands for -1, 0 and 1 of a unit, and for -2 and 2 of a day where it has one ("przedwczoraj"), unless `o.numeric`
asks for the number — the value within 1% of the whole number, as ICU has it. Otherwise the number is written by the
locale's [number_format](number_format/README.md) (three fraction digits at most) and the phrase chosen by its
[plural](plural.md) form as written: "za 1,5 dnia".

1. A value of a unit.
2. A span of time in the unit that fits it, rounded to a whole number of it: under a minute seconds, under an hour
   minutes, under a day hours, under a week days, under 30 days weeks, under 365 days months (of 30.44 days), else
   years (of 365.24 days). -90 minutes is "2 hours ago", -24 hours "yesterday".

## Parameters

| Parameter | Description |
|---|---|
| `value` | how many units from now, negative in the past |
| `unit` | a [time_unit](time_unit.md) |
| `d` | a [duration](../core/duration/README.md) from now, negative in the past |
| `l` | the locale |
| `o` | the width and whether to keep the number, [relative_options](relative_options.md) |

## Return value

The phrase.

## Complexity

Logarithmic in the number of locales.

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

using namespace std::chrono_literals;

int main() {
    auto pl = txt::locale("pl");
    println("{}", txt::format_relative(-1, txt::time_unit::day, pl));
    println("{}", txt::format_relative(3, txt::time_unit::day, pl));
    println("{}", txt::format_relative(1.5, txt::time_unit::hour, txt::locale("en")));
    println("{}", txt::format_relative(duration(-90min), txt::locale("de")));
}
```

Output:

```text
wczoraj
za 3 dni
in 1.5 hours
vor 2 Stunden
```

## See also

- [time_unit](time_unit.md)
- [relative_options](relative_options.md)
- [locale](locale/README.md)
- [sgcl::txt](README.md)
