[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::operator+=, operator-=, sgcl::time::operator+, operator- (sgcl::time::date)

```cpp
/*(1)*/ friend constexpr date operator+(date d, int n) noexcept;
/*(2)*/ friend constexpr date operator+(int n, date d) noexcept;
/*(3)*/ friend constexpr date operator-(date d, int n) noexcept;
/*(4)*/ friend constexpr int operator-(date a, date b) noexcept;
/*(5)*/ constexpr date& operator+=(int n) noexcept;
/*(6)*/ constexpr date& operator-=(int n) noexcept;
```

The day arithmetic in operators, as a [datetime](../datetime.md) has it. The operators other than the compound
assignments are hidden friends, found through a `date` argument.

1. `d.add_days(n)`.
2. The same, the number first.
3. The date `n` days before `d`, as `d.add_days(-n)` is, and for an `n` of `INT_MIN` too, whose negation an `int`
   does not hold.
4. The days from `b` to `a`, `b.days_until(a)`.
5. `*this = add_days(n)`.
6. `*this = *this - n`.

## Parameters

| Parameter | Description |
|---|---|
| `d`, `a`, `b` | the dates |
| `n` | the days to move by |

## Return value

- (1–3) The date moved, saturated at the ends of the calendar.
- (4) The number of days, negative when `a` is the earlier.
- (5–6) `*this`.

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
    time::date d(2026, 9, 24);
    println("{} {} {}", d + 7, 7 + d, d - 30);
    println("{}", time::date(2026, 12, 25) - d);
    d += 8;
    println(d);
    d -= 2;
    println(d);
}
```

Output:

```text
2026-10-01 2026-10-01 2026-08-25
92
2026-10-02
2026-09-30
```

## See also

- [add_days](add_days.md), [days_until](days_until.md): the named functions
- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [sgcl::time::date](../date.md)
