[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::hash

```cpp
size_t hash() const noexcept;
```

A hash of the node, equal for [equal](operator_cmp.md) nodes (`0x10` and `16` alike, a mapping's members in any
order): what `std::hash<encoding::yaml>` gives, so a node is a key of a [map](../../core/map/README.md).

## Parameters

None.

## Return value

The hash.

## Complexity

Linear in the size of the node.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::yaml::parse("0x10")->hash() == encoding::yaml(16).hash());
    println(encoding::yaml::parse("{a: 1, b: 2}")->hash() == encoding::yaml::parse("{b: 2, a: 1}")->hash());
}
```

Output:

```text
true
true
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::encoding::yaml](README.md)
