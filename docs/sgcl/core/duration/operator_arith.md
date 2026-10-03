[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::operator+=, operator-=, operator\*=, operator/=, operator%=, sgcl::operator+, operator-, operator\*, operator/, operator% (sgcl::duration)

```cpp
/*(1)*/ friend constexpr duration operator+(duration a, duration b) noexcept;
/*(2)*/ friend constexpr duration operator-(duration a, duration b) noexcept;
/*(3)*/ friend constexpr duration operator-(duration a) noexcept;
/*(4)*/ friend constexpr duration operator+(duration a) noexcept;
/*(5)*/ template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        friend constexpr duration operator*(duration d, I n) noexcept;
/*(6)*/ template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        friend constexpr duration operator*(I n, duration d) noexcept;
/*(7)*/ template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        friend constexpr duration operator/(duration d, I n) noexcept;
/*(8)*/ friend constexpr int64_t operator/(duration a, duration b) noexcept;
/*(9)*/ friend constexpr duration operator%(duration a, duration b) noexcept;
/*(10)*/ constexpr duration& operator+=(duration d) noexcept;
/*(11)*/ constexpr duration& operator-=(duration d) noexcept;
/*(12)*/ template<std::integral I>
         requires (!std::is_same_v<I, bool>)
         constexpr duration& operator*=(I n) noexcept;
/*(13)*/ template<std::integral I>
         requires (!std::is_same_v<I, bool>)
         constexpr duration& operator/=(I n) noexcept;
/*(14)*/ constexpr duration& operator%=(duration d) noexcept;
/*(15)*/ template<class Clock, class D>
         friend constexpr auto operator+(const std::chrono::time_point<Clock, D>& t, duration d)
             noexcept(std::is_arithmetic_v<typename D::rep>);
/*(16)*/ template<class Clock, class D>
         friend constexpr auto operator+(duration d, const std::chrono::time_point<Clock, D>& t)
             noexcept(std::is_arithmetic_v<typename D::rep>);
/*(17)*/ template<class Clock, class D>
         friend constexpr auto operator-(const std::chrono::time_point<Clock, D>& t, duration d)
             noexcept(std::is_arithmetic_v<typename D::rep>);
```

The arithmetic of durations, saturated at the ends of the range instead of wrapping: a result that would not fit is
`duration::max()` or `duration::min()`. Go wraps silently; `std::chrono` overflows as its integer does, which is
undefined. The operators other than the compound assignments are hidden friends, found through a `duration`
argument; a `std::chrono` duration of whole nanoseconds converts to one, so `d + 500ms` and `5s - d` are durations.

1. The sum.
2. The difference.
3. The negation; `-min()`, which has no positive counterpart, is `max()`.
4. The duration itself.
5. The product by an integer (not `bool`).
6. The same, the integer first.
7. The quotient by an integer, toward zero as an `int`'s division.
8. How many whole times `b` fits in `a`, toward zero: an `int64_t`, not a duration.
9. What is left of `a` after the whole `b`s, with the sign of `a`, as `%` of an `int`.
10. `*this = *this + d`.
11. `*this = *this - d`.
12. `*this = *this * n`.
13. `*this = *this / n`.
14. `*this = *this % d`.
15. The point `t` moved forward by `d`.
16. The same, the duration first.
17. The point `t` moved back by `d`.

A point of any clock moved by a duration (15–17), `steady_clock::now() + d` or a deadline minus a margin, is of the
clock's type as chrono makes it, in the finer of the two units (the common type of `D` and nanoseconds), and
saturated at that type's `max()` and `min()` when its count is a signed 64-bit integer: `now() + duration::max()` is
`time_point::max()`, which the timers of async read as never, where chrono's own `+` would overflow into the past
and fire at once. A point of a floating or an unsigned count is moved by chrono's own arithmetic.

A duration times a floating number does not compile: `duration(std::chrono::duration<double>(d.seconds() * 1.5))`
is the product by 1.5, truncated.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b`, `d` | the durations |
| `n` | the integer multiplied or divided by |
| `t` | the point moved |

## Return value

- (1–7), (9) The duration computed, saturated.
- (8) The whole quotient; `min() / -nanosecond`, the one quotient past the range, is the largest `int64_t`.
- (10–14) `*this`.
- (15–17) The point moved, a `std::chrono::time_point<Clock, std::common_type_t<D, std::chrono::nanoseconds>>`.

## Complexity

Constant.

## Exceptions

- (1–14) None. A division by zero (7–9, 13–14) is a division by zero, as for an `int`: not an exception.
- (15–17) None for a point of an arithmetic count; otherwise what the arithmetic of its count throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    duration d = 90 * minute;
    println("{} {} {}", d + 90 * second, d - 2 * hour, -d);
    println("{} {} {}", d * 3, d / 4, d / (7 * minute));
    println("{} {}", d % (7 * minute), -d % (7 * minute));
    println("{} {}", d + 500ms, d < 5s);  // with std::chrono durations

    duration total;
    for (int i : range(1, 5)) {
        total += i * 250 * millisecond;
    }
    println("{}", total);

    println("{}", duration::max() * 2 == duration::max());  // saturated
    auto now = std::chrono::steady_clock::now();
    println("{} {}", now + total > now, now + duration::max() == decltype(now)::max());
}
```

Output:

```text
1h31m30s -30m0s -1h30m0s
4h30m0s 22m30s 12
6m0s -6m0s
1h30m0.5s false
2.5s
true
true true
```

## See also

- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [max](max.md), [min](min.md): where the arithmetic saturates
- [sgcl::duration](../duration.md)
