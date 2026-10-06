[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::format_interval

```cpp
string format_interval(const datetime& to, const txt::locale& l, const string& skeleton) const;
```

Returns the datetime and a second one as an interval in a locale's patterns for a skeleton (CLDR's
`intervalFormats`, TR35 Part 4 §2.6.3), the second seen in this one's zone: "24–26 wrz 2026" for `yMMMd`,
"14:05–16:30" for `Hm`. The greatest field the two differ in (era, year, month, day, period, hour, minute) chooses
the locale's pattern for the closest interval skeleton; the pattern is written with the first time up to the first
field it repeats and with the second time from there. Two times the skeleton cannot tell apart are written once; a
difference in a field larger than the skeleton's (a change of the year under `MMMd`, of the day under `Hm`) adds the
fields down to the skeleton's and writes both times in full in the locale's fallback ("{0} – {1}"), as ICU does.

## Parameters

| Parameter | Description |
|---|---|
| `to` | the end of the interval |
| `l` | the locale |
| `skeleton` | the fields: `"yMMMd"`, `"jm"` |

## Return value

The text.

## Complexity

Linear in the number of the locale's patterns.

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

using namespace std::chrono_literals;

int main() {
    auto t = time::date(2026, 9, 24).at(14, 5, time::zone::utc());
    auto pl = txt::locale("pl");
    println("{}", t.format_interval(t + 48h, pl, "yMMMd"));
    println("{}", t.format_interval(t + 24h * 40, pl, "yMMMd"));
    println("{}", t.format_interval(t + 2h, pl, "Hm"));
    println("{}", t.format_interval(t + 3h, txt::locale("en"), "hm"));
}
```

Output:

```text
24–26 wrz 2026
24 wrz–3 lis 2026
14:05–16:05
2:05 – 5:05 PM
```

## See also

- [format](format.md)
- [date_format::from_skeleton](../date_format/from_skeleton.md)
- [sgcl::time::datetime](README.md)
