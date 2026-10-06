[sgcl](../../README.md) › [time](../README.md) › [date_format](README.md)

# sgcl::time::date_format::from_pattern

```cpp
static date_format from_pattern(const txt::locale& l, const string& pattern);
```

Returns a format of a pattern of CLDR's letters as it is, written with the locale's names and digits (LDML Part 4
§8): a run of one letter is a field, text between apostrophes is literal (`''` an apostrophe), the rest stands as it
is. Era `G`, year `y` `u` `Y`, quarter `Q` `q`, month `M` (inside a date) and `L` (standing alone), week `w` `W`,
day `d` `D` `F` `g`, weekday `E` `e` `c`, period `a` `b` `B`, hour `h` `H` `K` `k`, minute `m`, second `s`, fraction
`S`, `A`, zone `z` `Z` `O` `v` `V` `X` `x`: one to five letters as CLDR defines each. A letter CLDR does not define
there is written as it stands, as the `%` writer of the module writes a specifier it does not know.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the locale |
| `pattern` | the pattern: `"EEEE, d MMMM y"` |

## Return value

The format.

## Complexity

Constant.

## Exceptions

None.

## Notes

The zone's names — `z` `zzzz` `v` `vvvv` `VVV` `VVVV` (`CEST`, "Central European Summer Time", "czas: Polska") —
come from the optional headers of the [display names](../../txt/names.md) of the locale, as TR35 Part 4 §7 resolves
them (a metazone's name, a zone's own, the location format with the country or the exemplar city); where they are
not included the zone is written in the localized GMT format (`GMT+2`, `GMT+02:00`), and `VVV` is the last part of
the zone's identifier.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto t = time::date(2026, 9, 24).at(14, 5, time::zone::utc());
    println("{}", time::date_format::from_pattern(txt::locale("pl"), "EEEE, d MMMM y 'o' HH:mm")
                      .format(t));
    println("{}", time::date_format::from_pattern(txt::locale("pl"), "LLLL y").format(t));
    println("{}",
            time::date_format::from_pattern(txt::locale("en"), "QQQQ, 'week' w, h:mm B").format(t));
}
```

Output:

```text
czwartek, 24 września 2026 o 14:05
wrzesień 2026
3rd quarter, week 39, 2:05 in the afternoon
```

## See also

- [(constructor)](date_format.md)
- [sgcl::time::date_format](README.md)
