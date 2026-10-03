[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::to_int64

```cpp
optional<int64_t> to_int64() const noexcept;
```

The number as an `int64_t` when it fits, and nothing when it does not: Go's `Int64` and `IsInt64` in one call.

## Parameters

None.

## Return value

The value, or `nullopt` when it is below `INT64_MIN` or above `INT64_MAX`.

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
    math::big_integer least = INT64_MIN;
    println("{} {}", *least.to_int64(), (least - 1).to_int64().has_value());
    println("{}", math::big_integer(2).pow(64).to_int64().value_or(-1));
}
```

Output:

```text
-9223372036854775808 false
-1
```

## See also

- [to_uint64](to_uint64.md): as a `uint64_t`
- [to_double](to_double.md): the nearest `double`
- [sgcl::math::big_integer](../big_integer.md)
