[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::parse

```cpp
static expected<duration, duration_error> parse(const string& text) noexcept;
```

Reads Go's text of a duration: whatever Go's `time.ParseDuration` reads, and nothing else. A sign, then one or more
numbers, each with a unit and a fraction or none (`"1.5h"`, `".5s"`, `"1.s"`), in any order and repeated
(`"1h30m"`, `"-1.5h"`, `"2h45m0.5s"`); `"0"` alone is zero. The units are `ns`, `us` or `µs` (the micro sign or the
Greek mu), `ms`, `s`, `m` and `h`. No spaces and no days: a day of the calendar is 23, 24 or 25 hours, and that is a
[date's](../../time/date.md) `add_days`.

A fraction is taken exactly, however many digits it has, and cut to the nanosecond; Go multiplies it in `double`
and may land a nanosecond off. A text whose value does not fit in the range is an error, not a saturated
duration.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |

## Return value

The duration, or a [duration_error](../duration_error.md) with a sentence and the byte the reading stopped on:

- `"a number expected"`: an empty text, a sign alone, or a unit with no number before it;
- `"a unit expected: ns, us, ms, s, m or h"`: a number with no unit after it;
- `"an unknown unit: ns, us, ms, s, m or h expected"`: a unit that is none of these (`"1d"`, `"2 hours"`);
- `"out of range: a duration is at most about 292 years"`: a value past `max()` or `min()`.

## Complexity

Linear in the length of `text`.

## Exceptions

None.

## Notes

A text from outside the program (a setting, the user) is parsed, and its error is a value; a text the program
itself writes is constructed, `duration d("1h30m")` ([constructor](duration.md)), and a wrong one throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* text : {"1h30m", "-1.5h", ".5s", "1.000000001s", "300µs", "0"}) {
        println("{} -> {}", text, duration::parse(text).value());
    }
    for (const char* text : {"", "5", "1d", "1h 30m", "3000000h"}) {
        auto d = duration::parse(text);
        println("\"{}\": {} (byte {})", text, d.error().message(), d.error().offset());
    }
}
```

Output:

```text
1h30m -> 1h30m0s
-1.5h -> -1h30m0s
.5s -> 500ms
1.000000001s -> 1.000000001s
300µs -> 300µs
0 -> 0s
"": a number expected (byte 0)
"5": a unit expected: ns, us, ms, s, m or h (byte 1)
"1d": an unknown unit: ns, us, ms, s, m or h expected (byte 1)
"1h 30m": an unknown unit: ns, us, ms, s, m or h expected (byte 1)
"3000000h": out of range: a duration is at most about 292 years (byte 0)
```

## See also

- [to_string](to_string.md): writes the text
- [(constructor)](duration.md): a duration from a literal text
- [duration_error](../duration_error.md): why a text is not a duration
- [sgcl::duration](../duration.md)
