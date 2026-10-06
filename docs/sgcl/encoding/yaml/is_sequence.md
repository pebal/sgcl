[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::is_sequence

```cpp
bool is_sequence() const noexcept;
```

Whether the node is a sequence: one question of [type](type.md).

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
        print("{} ", e.is_sequence());
    }
    println();
}
```

Output:

```text
false false false false false true false 
```

## See also

- [type](type.md)
- [sgcl::encoding::yaml](README.md)
