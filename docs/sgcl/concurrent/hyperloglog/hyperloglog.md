[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::hyperloglog

```cpp
explicit hyperloglog(unsigned precision = 14);         // (1)
hyperloglog(const hyperloglog&) noexcept = default;    // (2)
hyperloglog(hyperloglog&&) noexcept = default;         // (3)
```

1. A sketch of 2^`precision` registers, every one zero. The relative standard error is 1.04/√2^*p*: 26% at 4, 3.3% at
   10, 0.81% at 14 (16 KB), 0.2% at 18 (256 KB).
2. A handle of the same sketch: the copy shares the registers.
3. The same, taken from the other handle, which stands for the same sketch still.

## Parameters

| Parameter | Description |
|---|---|
| `precision` | *p*, from 4 to 18 |

## Complexity

- (1) Linear in 2^*p*: the registers allocated, zero.
- (2–3) Constant.

## Exceptions

- (1) `invalid_argument` for a precision outside 4 to 18.
- (2–3) None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::hyperloglog fine(18);
    concurrent::hyperloglog same = fine;
    println("{} {}", same.precision(), same == fine);
}
```

Output:

```text
18 true
```

## See also

- [precision](precision.md)
- [sgcl::concurrent::hyperloglog](README.md)
