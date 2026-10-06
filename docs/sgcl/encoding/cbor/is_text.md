[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::is_text

```cpp
bool is_text() const noexcept;
```

Whether the value is a text string: one question of [type](type.md).

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
    println("{} {} {}", c.is_text(), c["a"].is_text(), c["a"][2].is_text());
}
```

Output:

```text
false false true
```

## See also

- [type](type.md)
- [sgcl::encoding::cbor](README.md)
