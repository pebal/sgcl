[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::operator==, operator\<=\> (sgcl::time::datetime)

```cpp
friend bool operator==(const datetime& a, const datetime& b) noexcept;                     // (1)
friend std::strong_ordering operator<=>(const datetime& a, const datetime& b) noexcept;    // (2)
```

Compare two instants, whatever the zones they are seen in: the same moment in Warsaw and in UTC is equal. `!=`, `<`,
`<=`, `>` and `>=` are made from these by the compiler; they are Go's `t.Equal(u)`, `t.Before(u)` and `t.After(u)`.
Go's `==` compares the zone too, which is a known trap there; here `a.zone() == b.zone()` asks that other question.

1. `true` when the two are the same instant.
2. The order of the two instants.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the datetimes compared |

## Return value

- (1) Whether the instants are equal.
- (2) `std::strong_ordering::less`, `equal` or `greater`.

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
    auto meeting = time::date(2026, 10, 30).at(9, 30, time::zone("America/New_York"));
    auto there = meeting.in(time::zone("Europe/Warsaw"));
    println("{} {}", meeting == there, meeting.zone() == there.zone());
    println("{} {}", meeting < meeting + second, meeting.utc() >= there);
}
```

Output:

```text
true false
true true
```

## See also

- [operator+, operator-](operator_arith.md): the arithmetic
- [zone](zone.md): the zone, compared apart
- [sgcl::time::datetime](README.md)
