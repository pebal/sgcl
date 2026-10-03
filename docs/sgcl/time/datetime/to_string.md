[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::to_string, sgcl::time::operator\<\< (sgcl::time::datetime)

```cpp
string to_string() const noexcept;                                                             // (1)
template<class CharT, class Traits>
friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,    // (2)
                                                     const datetime& t);
```

1. RFC 3339 with the fraction of a second only where there is one, its trailing zeros left out, and `Z` for an
   offset of zero: Go's `RFC3339Nano`, `2026-09-24T12:41:15.122575+02:00`, `2026-09-24T10:41:15.5Z`. An offset with
   seconds (the local mean times of the 19th century) is written to the minute, as Go writes it; RFC 3339 has no
   seconds there. The other texts are the [layouts](../layout.md) and the patterns of [format](format.md).
2. Writes `t.to_string()` to `os`.

## Parameters

| Parameter | Description |
|---|---|
| `os` | the stream written to |
| `t` | the datetime written |

## Return value

- (1) The text.
- (2) `os`.

## Complexity

Logarithmic in the number of the zone's changes; constant in UTC and in a fixed zone.

## Exceptions

- (1) None.
- (2) What the stream throws when its exceptions are on.

## Notes

[parse](parse.md) with `time::rfc3339` or `time::rfc3339_nano` reads back every text `to_string` writes but one with
an offset cut to the minute, and [txt::format](../../txt/format.md) writes a datetime as `to_string()` does under
`{}` ([README: Formatting with txt](../README.md#formatting-with-txt)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <iostream>

using namespace sgcl;

int main() {
    auto t = time::date(2026, 9, 24).at(12, 41, 15, time::zone("Europe/Warsaw"));
    println("{} {}", t.to_string(), t + 122575 * microsecond);
    println("{}", (t + 500 * millisecond).utc());
    auto old = time::datetime::from_unix(-2500000000, time::zone("Europe/Brussels"));
    println("{} {}", old, old.offset());
    std::cout << t << '\n';
}
```

Output:

```text
2026-09-24T12:41:15+02:00 2026-09-24T12:41:15.122575+02:00
2026-09-24T10:41:15.5Z
1890-10-11T19:50:50+00:17 17m30s
2026-09-24T12:41:15+02:00
```

## See also

- [format](format.md): the text by a layout or a pattern
- [parse](parse.md): reads the text back
- [sgcl::time::datetime](../datetime.md)
