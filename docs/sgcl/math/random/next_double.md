[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::next_double

```cpp
double next_double() noexcept;
```

A double in `[0, 1)`, in steps of 2^-53: the low 53 bits of a draw over 2^53, as Go's `Float64`, so from the same
stream the same numbers. Every one of the 2^53 values is as likely, and 1 is never drawn.

## Parameters

None.

## Return value

The double drawn.

## Complexity

Constant: one draw.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    println("{} {}", r.next_double(), r.next_double());

    int inside = 0;
    for (int i : range(100000)) {
        double x = r.next_double();
        double y = r.next_double();
        inside += x * x + y * y < 1;
    }
    println("pi is about {:.3f}", 4.0 * inside / 100000);
}
```

Output:

```text
0.755108222592302 0.987070245086441
pi is about 3.141
```

## See also

- [next_int](next_int.md): a whole number in a range
- [next_normal](next_normal.md), [next_exponential](next_exponential.md): other distributions
- [sgcl::math::random](README.md)
