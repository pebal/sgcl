[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::truncate

```cpp
constexpr duration truncate(duration step) const noexcept;
```

The duration toward zero to a multiple of `step`: Go's `Truncate`, and what `duration_cast` or `floor` to a unit is
for a positive `std::chrono` duration. A step of zero or less leaves the duration as it is.

## Parameters

| Parameter | Description |
|---|---|
| `step` | the multiple to truncate to |

## Return value

The largest multiple of `step` not further from zero than the duration, with its sign; the duration itself when
`step` is zero or less.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    duration d = 1234567 * nanosecond;
    println("{} {}", d.truncate(millisecond), d.truncate(microsecond));
    println("{}", (-d).truncate(millisecond));  // toward zero
    println("{}", d.truncate(duration()));  // a step of zero: as it is
}
```

Output:

```text
1ms 1.234ms
-1ms
1.234567ms
```

## See also

- [round](round.md): to the nearest multiple
- [sgcl::duration](README.md)
