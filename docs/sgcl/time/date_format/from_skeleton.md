[sgcl](../../README.md) › [time](../README.md) › [date_format](README.md)

# sgcl::time::date_format::from_skeleton

```cpp
static date_format from_skeleton(const txt::locale& l, const string& skeleton);
```

Returns the locale's format for the fields of a skeleton: the letters of the fields wanted, in any order and in the
widths wanted (`"yMMMd"`, `"MMMMd"`, `"jm"`), matched to the locale's own pattern for them as ICU's
DateTimePatternGenerator matches them (TR35 Part 4 §2.6.2). The candidates are the locale's `availableFormats` and
its eight patterns of the styles, each under the skeleton of its own fields; the closest by field and width wins,
`availableFormats` on a tie; the widths are then made the ones asked for where the pattern's width is the matched
skeleton's (a pattern that pads, `dd.MM.y` for `yMd`, keeps its padding), and `E` is `EEE`. A skeleton with a date
and a time that no pattern has both of is matched as a date and a time apart and joined as [style](../style.md)
joins them, by the date's length. A field still missing is appended by the locale's `appendItems` (`"{0} ({2}:
{1})"`, the field's name quoted). `j` is the locale's hour, 12 or 24 (CLDR's `timeData`), `J` the same without a
period, `C` its first allowed form; fractions of a second (`S`) follow the seconds after the locale's decimal
separator. A skeleton of no fields is an empty pattern.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the locale |
| `skeleton` | the fields |

## Return value

The format.

## Complexity

Linear in the number of the locale's patterns.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto tag : {"pl", "en", "de", "ja"}) {
        txt::locale l(tag);
        println("{}: {} | {} | {}", tag, time::date_format::from_skeleton(l, "yMMMd").pattern(),
                time::date_format::from_skeleton(l, "MMMMEd").pattern(),
                time::date_format::from_skeleton(l, "jm").pattern());
    }
}
```

Output:

```text
pl: d MMM y | EEE, d MMMM | HH:mm
en: MMM d, y | EEE, MMMM d | h:mm a
de: d. MMM y | EEE, d. MMMM | HH:mm
ja: y年M月d日 | M月d日(EEE) | H:mm
```

## See also

- [(constructor)](date_format.md)
- [pattern](pattern.md)
- [sgcl::time::date_format](README.md)
