[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::operator==, operator\<=\> (sgcl::duration)

```cpp
friend constexpr bool operator==(const duration&, const duration&) noexcept = default;    // (1)
friend constexpr std::strong_ordering operator<=>(const duration&,                        // (2)
                                                 const duration&) noexcept = default;
```

Compare two durations by their nanoseconds. `!=`, `<`, `<=`, `>` and `>=` are made from these by the compiler. A
`std::chrono` duration of whole nanoseconds converts to a duration, so `d < 5s` and `d == 1500ms` compare as
durations.

1. `true` when the two hold the same number of nanoseconds.
2. The order of the two counts.

## Parameters

The two durations compared, the operands, unnamed in the declarations.

## Return value

- (1) Whether the durations are equal.
- (2) `std::strong_ordering::less`, `equal` or `greater`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    duration lap = 1500 * millisecond;
    println("{} {} {}", lap == 1500ms, lap < 2 * second, lap >= 1s);
    println("{}", -lap < duration::zero());
}
```

Output:

```text
true true true
true
```

## See also

- [operator+](operator_arith.md): the arithmetic
- [sgcl::duration](README.md)
