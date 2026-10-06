[sgcl](../../README.md) › [time](../README.md)

# sgcl::time::date_format

```cpp
#include "sgcl/time/localized.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class date_format;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::time::date_format` is a locale's way of writing dates and times, resolved once: a pattern of CLDR's letters
(`"d MMMM y, HH:mm"`) with the locale's names of months, days, eras and periods of the day and its digits, from the
data of CLDR 46 for every locale. It is made from the locale's four lengths of patterns ([style](../style.md)), from
a skeleton matched to the locale's own patterns, or from a pattern as given. The calendar is the time module's, the
proleptic Gregorian one. [datetime::format](../datetime/format.md) and [date::format](../date/format.md) are its
one-line forms; [datetime::format_interval](../datetime/format_interval.md) writes intervals.

## Rules

- It holds the pattern as a [string](../../core/string/README.md); [format](format.md) is `const` and may be
  called from many threads at once.
- Nothing fails: a letter CLDR does not define is written as it stands, a skeleton is always matched to something.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](date_format.md) | constructs the format of a locale's styles |
| [from_skeleton](from_skeleton.md) | the locale's pattern for the fields of a skeleton (static) |
| [from_pattern](from_pattern.md) | a pattern of CLDR's letters as given (static) |

#### Formatting

| Function | Description |
|---|---|
| [format](format.md) | the text of a datetime or a date |

#### Observers

| Function | Description |
|---|---|
| [pattern](pattern.md) | the pattern resolved |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto f = time::date_format::from_skeleton(txt::locale("pl"), "MMMMEEEEd");
    for (int d : {24, 25, 26}) {
        println("{}", f.format(time::date(2026, 9, d)));
    }
}
```

Output:

```text
czwartek, 24 września
piątek, 25 września
sobota, 26 września
```

## See also

- [style](../style.md)
- [datetime::format](../datetime/format.md)
- [to_string](../to_string.md): the names of months and days
- [txt::locale](../../txt/locale/README.md)
- [sgcl::time](../README.md)
