[sgcl](../../README.md) › [time](../README.md) › [date_format](README.md)

# sgcl::time::date_format::format

```cpp
string format(const datetime& t) const;    // (1)
string format(const date& d) const;        // (2)
```

Returns the text of a time or a date in the format.

1. The fields of the [datetime](../datetime/README.md) in its zone: the zone's offset and identifier for the zone's
   letters.
2. The fields of the [date](../date/README.md); where the pattern asks for a time, its midnight in UTC.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the time |
| `d` | the date |

## Return value

The text.

## Complexity

Linear in the length of the pattern.

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    time::date_format f(txt::locale("de"), time::style::full, time::style::medium);
    println("{}", f.format(time::date(2026, 9, 24).at(14, 5, time::zone::utc())));
    println("{}", time::date_format(txt::locale("ru"), time::style::detailed)
                      .format(time::date(2026, 5, 1)));
}
```

Output:

```text
Donnerstag, 24. September 2026 um 14:05:00
1 мая 2026 г.
```

## See also

- [datetime::format](../datetime/format.md)
- [sgcl::time::date_format](README.md)
