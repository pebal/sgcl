[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set/README.md) › [node_type](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type::node_type

```cpp
node_type() noexcept;                     // (1)
node_type(node_type&& other) noexcept;    // (2)
```

Constructs a node handle.

1. An empty handle: it holds no node.
2. Takes the node of `other` over; `other` is empty after.

The handle is not copyable: one node has one owner. A handle that holds a node comes from
[extract](../ordered_set/extract.md), or from an [insert](../ordered_set/insert.md) that found the element there.

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
    using node = ordered_set<string>::node_type;
    node empty;
    println("{}", empty.empty());

    ordered_set<string> s = {"one"};
    node taken = s.extract("one");
    node moved = std::move(taken);
    println("{} {}", taken.empty(), moved.value());

    println("{}", std::is_copy_constructible_v<node>);
}
```

Output:

```text
true
true one
false
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [extract](../ordered_set/extract.md): takes a node out of a set
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type](README.md)
