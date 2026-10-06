[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::is_integer

```cpp
bool is_integer() const noexcept;
```

Whether the value is an integer: one question of [type](type.md).

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
    println("{} {} {}", c.is_integer(), c["a"].is_integer(), c["a"][2].is_integer());
}
```

Output:

```text
false false false
```

## See also

- [type](type.md)
- [sgcl::encoding::cbor](README.md)
