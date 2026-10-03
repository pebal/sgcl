[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md) › [node_type](../ordered_map-node_type.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type::node_type

```cpp
node_type() noexcept;                     // (1)
node_type(node_type&& other) noexcept;    // (2)
```

Constructs a node handle.

1. An empty handle: it holds no node.
2. Takes the node of `other` over; `other` is empty after.

The handle is not copyable: one node has one owner. A handle that holds a node comes from
[extract](../ordered_map/extract.md), or from an [insert](../ordered_map/insert.md) that found the key there.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle the node is taken from |

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
    using node = ordered_map<int, string>::node_type;
    node empty;
    println("{}", empty.empty());

    ordered_map<int, string> m = {{1, "one"}};
    node taken = m.extract(1);
    node moved = std::move(taken);
    println("{} {} {}", taken.empty(), moved.key(), moved.mapped());

    println("{}", std::is_copy_constructible_v<node>);
}
```

Output:

```text
true
true 1 one
false
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [extract](../ordered_map/extract.md): takes a node out of a map
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type](../ordered_map-node_type.md)
