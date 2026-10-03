[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::to_uint64

```cpp
optional<uint64_t> to_uint64() const noexcept;
```

The number as a `uint64_t` when it fits, and nothing when it does not: Go's `Uint64` and `IsUint64` in one call. A
negative number never fits.

## Parameters

None.

## Return value

The value, or `nullopt` when it is negative or above `UINT64_MAX`.

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
    math::big_integer most = UINT64_MAX;
    println("{} {}", *most.to_uint64(), (most + 1).to_uint64().has_value());
    println("{}", math::big_integer(-1).to_uint64().has_value());
}
```

Output:

```text
18446744073709551615 false
false
```

## See also

- [to_int64](to_int64.md): as an `int64_t`
- [to_bytes](to_bytes.md): the magnitude as bytes
- [sgcl::math::big_integer](../big_integer.md)
