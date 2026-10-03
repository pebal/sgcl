[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::min

```cpp
static constexpr result_type min() noexcept;
```

The smallest value [operator()](operator_call.md) gives, 0: what the standard library asks of a uniform random bit
generator, beside [max](max.md).

## Parameters

None.

## Return value

0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::random::min(), math::random::max());
}
```

Output:

```text
0 18446744073709551615
```

## See also

- [max](max.md): the largest value of a draw
- [sgcl::math::random](README.md)
