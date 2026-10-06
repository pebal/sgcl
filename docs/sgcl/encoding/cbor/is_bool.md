[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::is_bool

```cpp
bool is_bool() const noexcept;
```

Whether the value is a boolean: one question of [type](type.md).

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
    println("{} {} {}", c.is_bool(), c["a"].is_bool(), c["a"][2].is_bool());
}
```

Output:

```text
false false false
```

## See also

- [type](type.md)
- [sgcl::encoding::cbor](README.md)
