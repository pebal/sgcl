[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::max

```cpp
static constexpr result_type max() noexcept;
```

The largest value [operator()](operator_call.md) gives, 2^64 - 1: every word of the stream is a draw, all 64 bits.
What the standard library asks of a uniform random bit generator, beside [min](min.md).

## Parameters

None.

## Return value

`std::numeric_limits<uint64_t>::max()`.

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
    println("{:#x}", math::random::max());
    println("{}", math::random::max() == UINT64_MAX);
}
```

Output:

```text
0xffffffffffffffff
true
```

## See also

- [min](min.md): the smallest value of a draw
- [sgcl::math::random](README.md)
