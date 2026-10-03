[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::to_string, sgcl::time::operator\<\< (sgcl::time::date)

```cpp
string to_string() const noexcept;                                                             // (1)
template<class CharT, class Traits>
friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,    // (2)
                                                     date d);
```

1. ISO 8601's extended calendar date, as `std::format`'s `%F` writes it: `"2026-09-24"`. The year has four digits
   at least and a minus sign when it is negative: `"-0044-03-15"`, `"10000-01-01"`.
2. Writes `d.to_string()` to `os`.

## Parameters

| Parameter | Description |
|---|---|
| `os` | the stream written to |
| `d` | the date written |

## Return value

- (1) The text, at most 12 bytes (`"-32767-12-31"`).
- (2) `os`.

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) What the stream throws when its exceptions are on.

## Notes

[parse](parse.md) reads back every text `to_string` writes, and [txt::format](../README.md#formatting-with-txt)
writes a date as `to_string()` does with `{}`, in a field of any width (`{:>12}`).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <iostream>

using namespace sgcl;

int main() {
    string text = time::date(2026, 9, 24).to_string();
    println("{} {} {}", text, time::date(-44, 3, 15), time::date(10000, 1, 1));
    std::cout << time::date(1, 1, 1) << '\n';
}
```

Output:

```text
2026-09-24 -0044-03-15 10000-01-01
0001-01-01
```

## See also

- [format](format.md): the date in a pattern
- [parse](parse.md): reads the text back
- [sgcl::time::date](README.md)
