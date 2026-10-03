[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::to_string, sgcl::operator\<\< (sgcl::duration)

```cpp
/*(1)*/ string to_string() const noexcept;
/*(2)*/ template<class CharT, class Traits>
        friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                                             duration d);
```

1. Go's text of the duration, as Go's `String()` writes it. The hours, minutes and seconds with the seconds'
   fraction, the leading units that are zero left out (`"1h0m0s"`, `"2m3.5s"`); a span under a second in the
   largest unit that keeps its first digit above zero (`"300ms"`, `"1.5µs"`, `"7ns"`), the micro sign as Go writes
   it; zero is `"0s"`; a negative duration has a `-` first. The fraction has no trailing zeros.
2. Writes `d.to_string()` to `os`. A `std::chrono` duration writes its count and its unit (`1500000ns`); a
   `duration` writes Go's text (`1.5ms`).

## Parameters

| Parameter | Description |
|---|---|
| `os` | the stream written to |
| `d` | the duration written |

## Return value

- (1) The text, at most 25 bytes (the smallest duration, `"-2562047h47m16.854775808s"`).
- (2) `os`.

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) What the stream throws when its exceptions are on.

## Notes

[parse](parse.md) reads back every text `to_string` writes. [txt::format](../../txt/format.md) writes a
duration as `to_string()` does, in a field of any width (`{:>12}`).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iostream>

using namespace sgcl;

int main() {
    println("{} {} {}", (90 * minute).to_string(), 123500 * millisecond, 300 * millisecond);
    println("{} {} {}", 1500 * nanosecond, 7 * nanosecond, duration());
    println("{} {}", -(2 * minute), duration::min());
    std::cout << 1500 * microsecond << '\n';
}
```

Output:

```text
1h30m0s 2m3.5s 300ms
1.5µs 7ns 0s
-2m0s -2562047h47m16.854775808s
1.5ms
```

## See also

- [parse](parse.md): reads the text back
- [sgcl::duration](../duration.md)
