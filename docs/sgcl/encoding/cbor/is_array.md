[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::is_array

```cpp
bool is_array() const noexcept;
```

Whether the value is an array: one question of [type](type.md).

## Parameters

None.

## Return value

`true` when it is.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor c = encoding::cbor::map({{"a", encoding::cbor::array({1, 1.5, "x"})}});
    println("{} {} {}", c.is_array(), c["a"].is_array(), c["a"][2].is_array());
}
```

Output:

```text
false true false
```

## See also

- [type](type.md)
- [sgcl::encoding::cbor](README.md)
