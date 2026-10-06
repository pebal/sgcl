[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::format

```cpp
string format(const string& pattern) const noexcept;                      // (1)
string format(const txt::locale& l, style date = style::medium) const;    // (2)
string format(const txt::locale& l, const string& skeleton) const;        // (3)
```

The text of the date.

1. By a [pattern](../README.md#patterns) of `std::format`'s specifiers for `<chrono>`, as libc++'s `std::format`
   writes them: `"%A, %d %B %Y"` is `"Thursday, 24 September 2026"`. Names are English. The writer never fails: a
   specifier of a time of day or of a zone, which a date does not answer, and one it does not know are written as
   they stand.
2. As a locale writes it, in its pattern of the length `date` ([style](../style.md)), with its names and digits
   (CLDR 46): `d.format(txt::locale("pl"))` is `"24 wrz 2026"`.
3. As a locale writes the fields of a skeleton, in any order: `d.format(l, "MMMMd")` is `"24 września"`
   ([date_format::from_skeleton](../date_format/from_skeleton.md)); a field of a time is its midnight in UTC.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern of `%` specifiers and the text between them |
| `l` | the locale |
| `date` | the length of the pattern, a [style](../style.md) |
| `skeleton` | the fields wanted: `"yMMMd"`, `"MMMMEd"` |

## Return value

The text.

## Complexity

- (1) Linear in the length of `pattern`.
- (2–3) Linear in the number of the locale's patterns.

## Exceptions

None.

## Notes

[txt::format](../README.md#formatting-with-txt) writes a date by the same pattern in a field,
`txt::format("{:%d.%m}", d)` or `{:>12%F}`; there the pattern is checked where the program is compiled, and
`{:%H}` of a date does not compile.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24);
    println(d.format("%A, %d %B %Y"));
    println(d.format("%d.%m.%y %G-W%V-%u %j %a %b"));
    println(d.format("%F %H:%M"));
    println("[{:%d.%m}] [{:>12%F}]", d, d);

    // In a locale
    {
        time::date d(2026, 9, 24);
        println("{} | {}", d.format(txt::locale("pl")),
                d.format(txt::locale("pl"), time::style::full));
        println("{} | {}", d.format(txt::locale("de"), "MMMMd"),
                d.format(txt::locale("ja"), "yMMMEd"));
    }
}
```

Output:

```text
Thursday, 24 September 2026
24.09.26 2026-W39-4 267 Thu Sep
2026-09-24 %H:%M
[24.09] [  2026-09-24]
24 wrz 2026 | czwartek, 24 września 2026
24. September | 2026年9月24日(木)
```

## See also

- [date_format](../date_format/README.md): a locale's format kept, and its patterns
- [to_string](to_string.md): ISO 8601's text
- [parse](parse.md): reads a date in a pattern
- [Patterns](../README.md#patterns): every specifier
- [sgcl::time::date](README.md)
