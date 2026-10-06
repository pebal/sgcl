[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::is_integer

```cpp
bool is_integer() const noexcept;
```

Whether the node is an integer: one question of [type](type.md).

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
    auto v = encoding::yaml::parse("[~, true, 7, 2.5, x, [1], {k: v}]").value();
    for (const auto& e : v.elements()) {
        print("{} ", e.is_integer());
    }
    println();
}
```

Output:

```text
false false true false false false false 
```

## See also

- [type](type.md)
- [sgcl::encoding::yaml](README.md)
