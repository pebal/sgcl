[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map/README.md) › [node_type](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type::node_type

```cpp
node_type() noexcept;                     // (1)
node_type(node_type&& other) noexcept;    // (2)
```

Constructs a node handle. A handle that owns a node is made by [extract](../sorted_map/extract.md); a handle is
not copyable.

1. An empty handle, owning no node.
2. Takes the node of `other` over; `other` is empty after.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose node to take |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    sorted_map<int, string>::node_type none;
    println("{}", none.empty());

    sorted_map<int, string> m = {{1, "one"}};
    auto nh = m.extract(1);
    auto moved = std::move(nh);
    println("{} {} {}", nh.empty(), moved.key(), moved.mapped());

    println("{}", std::is_copy_constructible_v<sorted_map<int, string>::node_type>);
}
```

Output:

```text
true
true 1 one
false
```

## See also

- [operator=](operator_assign.md): takes another handle's node
- [sgcl::sorted_map\<Key, T, Compare\>::node_type](README.md)
