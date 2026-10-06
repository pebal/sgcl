[sgcl](../README.md) › [core](README.md)

# sgcl::rounding

```cpp
#include "sgcl/core/rounding.h"   // or "sgcl/core.h"

namespace sgcl {
    enum class rounding : uint8_t {
        half_even,
        half_up,
        half_down,
        up,
        down,
        ceiling,
        floor,
        unnecessary
    };
}
```

How a number is rounded to fewer digits, one enumeration for every module that rounds: the
[decimal](../math/decimal/README.md) of math — its [div](../math/decimal/div.md),
[div_precision](../math/decimal/div_precision.md), [rescale](../math/decimal/rescale.md),
[round_precision](../math/decimal/round_precision.md), [sqrt](../math/decimal/sqrt.md),
[to_big_integer](../math/decimal/to_big_integer.md) and its constructor from a fraction — and txt's formatting of
numbers by locale. Every function that rounds takes it as its last argument, `half_even` when none is given — the
rounding of IEEE 754, of CLDR, of Python's `decimal` and of the banks, which does not lean either way over many
roundings. The names are Java's `RoundingMode` and Python's `ROUND_*`; "up" and "down" are away from zero and towards
it, `ceiling` and `floor` towards the infinities. The enumeration is in core, which every module sees, so that math
and txt share it.

| Value | Description |
|---|---|
| `half_even` | to the nearest, a tie to the even last digit: 2.5 → 2, 3.5 → 4, -2.5 → -2 |
| `half_up` | to the nearest, a tie away from zero: 2.5 → 3, -2.5 → -3 (the school's rounding, PostgreSQL's `round`) |
| `half_down` | to the nearest, a tie towards zero: 2.5 → 2, -2.5 → -2 |
| `up` | away from zero whenever anything is dropped: 2.1 → 3, -2.1 → -3 |
| `down` | towards zero, the digits dropped: 2.9 → 2, -2.9 → -2 |
| `ceiling` | towards +infinity: 2.1 → 3, -2.9 → -2 |
| `floor` | towards -infinity: 2.9 → 2, -2.1 → -3 |
| `unnecessary` | none: the result must be exact; one that is not is reported by the operation (`domain_error` from decimal) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (auto mode : {rounding::half_even, rounding::half_up, rounding::half_down, rounding::up,
                      rounding::down, rounding::ceiling, rounding::floor}) {
        print("{:>3}", math::decimal("2.5").rescale(0, mode));
        println("{:>4}", math::decimal("-2.5").rescale(0, mode));
    }
    try {
        math::decimal("2.5").rescale(0, rounding::unnecessary);
    } catch (const domain_error& e) {
        println(e.what());
    }
}
```

Output:

```text
  2  -2
  3  -3
  2  -2
  3  -3
  2  -2
  3  -2
  2  -3
sgcl::math::decimal: rounding::unnecessary, and the result is not exact
```

## See also

- [decimal](../math/decimal/README.md): what math rounds
- [rescale](../math/decimal/rescale.md): a decimal to a number of places
- [README: core](README.md)
