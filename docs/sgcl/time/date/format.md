[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::format

```cpp
string format(const string& pattern) const noexcept;
```

The date written by a [pattern](../README.md#patterns) of `std::format`'s specifiers for `<chrono>`, as libc++'s
`std::format` writes them: `"%A, %d %B %Y"` is `"Thursday, 24 September 2026"`. Names are English. The writer never
fails: a specifier of a time of day or of a zone, which a date does not answer, and one it does not know are
written as they stand.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern of `%` specifiers and the text between them |

## Return value

The text.

## Complexity

Linear in the length of `pattern`.

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

using namespace sgcl;

int main() {
    time::date d(2026, 9, 24);
    println(d.format("%A, %d %B %Y"));
    println(d.format("%d.%m.%y %G-W%V-%u %j %a %b"));
    println(d.format("%F %H:%M"));
    println("[{:%d.%m}] [{:>12%F}]", d, d);
}
```

Output:

```text
Thursday, 24 September 2026
24.09.26 2026-W39-4 267 Thu Sep
2026-09-24 %H:%M
[24.09] [  2026-09-24]
```

## See also

- [to_string](to_string.md): ISO 8601's text
- [parse](parse.md): reads a date in a pattern
- [Patterns](../README.md#patterns): every specifier
- [sgcl::time::date](README.md)
