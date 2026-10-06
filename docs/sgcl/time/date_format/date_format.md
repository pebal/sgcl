[sgcl](../../README.md) › [time](../README.md) › [date_format](README.md)

# sgcl::time::date_format::date_format

```cpp
explicit date_format(const txt::locale& l, style date = style::medium, style time = style::none);
```

Constructs a locale's format of a date and a time in its patterns of the lengths `date` and `time`
([style](../style.md)): the date's, the time's, or both joined by the pattern of the date's length — its `atTime`
form for `full` and `detailed` where the locale has one ("Thursday, September 24, 2026 at 2:05 PM"), as ICU joins
them. Both `none` is a format of an empty pattern.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the locale |
| `date` | the date's length |
| `time` | the time's length |

## Complexity

Logarithmic in the number of locales.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    time::date_format f(txt::locale("pl"), time::style::full, time::style::brief);
    println("{}", f.pattern());
    println("{}", f.format(time::date(2026, 9, 24).at(14, 5, time::zone::utc())));
}
```

Output:

```text
EEEE, d MMMM y HH:mm
czwartek, 24 września 2026 14:05
```

## See also

- [from_skeleton](from_skeleton.md)
- [from_pattern](from_pattern.md)
- [sgcl::time::date_format](README.md)
