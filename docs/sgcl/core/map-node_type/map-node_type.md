[sgcl](../../README.md) › [core](../README.md) › [map](../map/README.md) › [node_type](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::node_type::node_type

```cpp
node_type() noexcept;                     // (1)
node_type(node_type&& other) noexcept;    // (2)
```

Constructs a node handle.

1. An empty handle, which holds no node.
2. Takes the node of `other` over; `other` is empty after.

A handle that holds a node is made by [extract](../map/extract.md); there is no copy constructor.

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
    using handle = map<int, string>::node_type;
    handle none;
    println("{}", none.empty());

    map<int, string> m = {{1, "one"}};
    handle taken = m.extract(1);
    handle moved = std::move(taken);
    println("{} {} {}", taken.empty(), moved.key(), moved.mapped());
    println("{}", std::is_copy_constructible_v<handle>);
}
```

Output:

```text
true
true 1 one
false
```

## See also

- [operator=](operator_assign.md): takes another handle's node over
- [extract](../map/extract.md): unlinks an element into a node handle
- [sgcl::map\<Key, T, Hash, KeyEqual\>::node_type](README.md)
