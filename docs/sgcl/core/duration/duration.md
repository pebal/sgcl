[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::duration

```cpp
constexpr duration() noexcept = default;                                                // (1)
template<class Rep, class Period>
requires std::is_integral_v<Rep> && (std::ratio_divide<Period, std::nano>::den == 1)
constexpr duration(std::chrono::duration<Rep, Period> d) noexcept;                      // (2)
template<class Rep, class Period>
requires std::is_floating_point_v<Rep>
explicit constexpr duration(std::chrono::duration<Rep, Period> d) noexcept;             // (3)
explicit duration(const string& text);                                                  // (4)
```

Constructs a duration.

1. Zero.
2. From a `std::chrono::duration` of an integral count whose unit is a whole number of nanoseconds (`1h`, `30min`,
   `500ms`, `7ns`, a `std::chrono::seconds`): implicit and exact, saturated at `max()` or `min()` when the count
   does not fit in 64 bits of nanoseconds. A unit finer than a nanosecond does not convert: it takes a
   `duration_cast<std::chrono::nanoseconds>` first.
3. From a `std::chrono::duration` of a floating count, which may carry a part of a nanosecond: explicit, truncated
   toward zero as `duration_cast` does, saturated at the ends of the range, a NaN zero.
4. From Go's text, a literal the program itself spells (`duration("1h30m")`): what [parse](parse.md) reads, or
   `bad_expected_access<duration_error>` with `parse`'s message. A text from outside the program is parsed, and
   its error is a value.

A duration never comes from a bare number: `duration(5)` does not compile, `5 * second` does.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the `std::chrono` duration converted |
| `text` | Go's text of a duration |

## Complexity

- (1–3) Constant.
- (4) Linear in the length of `text`.

## Exceptions

- (1–3) None.
- (4) `bad_expected_access<duration_error>` when `text` is not a duration; its `error()` is the
  [duration_error](../duration_error/README.md) of `parse`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    duration none;
    duration lap = 1500ms;  // implicit, exact
    duration week = std::chrono::weeks(1);
    duration half(std::chrono::duration<double>(0.5));  // explicit, truncated
    duration spelled("2h45m");
    println("{} {} {} {} {}", none, lap, week, half, spelled);

    try {
        duration wrong("2 hours");
    } catch (const bad_expected_access<duration_error>& e) {
        println("{} (byte {})", e.error().message(), e.error().offset());
    }
}
```

Output:

```text
0s 1.5s 168h0m0s 500ms 2h45m0s
an unknown unit: ns, us, ms, s, m or h expected (byte 1)
```

## See also

- [parse](parse.md): reads a text from outside the program
- [operator std::chrono::nanoseconds](operator_conv.md): the conversion the other way
- [sgcl::duration](README.md)
