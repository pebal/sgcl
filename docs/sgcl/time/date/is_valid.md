[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::is_valid

```cpp
static constexpr bool is_valid(int year, int month, int day) noexcept;            // (1)
static constexpr bool is_valid(int year, time::month month, int day) noexcept;    // (2)
```

Checks whether a date exists as written: a year from -32767 to 32767, a month from 1 to 12 and a day within the
month. A date made of the same numbers ([constructor](date.md)) carries what is outside its range into the next
month or year; `is_valid` tells the numbers that need no carrying, for numbers that came from a person.

## Parameters

| Parameter | Description |
|---|---|
| `year` | the year |
| `month` | the month, as a number or a [month](../month.md) |
| `day` | the day of the month |

## Return value

`true` when the date exists, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    println("{} {}", time::date::is_valid(2026, 2, 29), time::date::is_valid(2024, 2, 29));
    println("{} {}", time::date::is_valid(2026, 13, 1), time::date::is_valid(32768, 1, 1));
    println("{}", time::date::is_valid(2026, time::month::september, 30));
}
```

Output:

```text
false true
false false
true
```

## See also

- [(constructor)](date.md): a date from numbers, carried
- [parse](parse.md): refuses a text of a date that does not exist
- [sgcl::time::date](../date.md)
