[sgcl](../../README.md) › [core](../README.md) › [set](../set.md) › [node_type](../set-node_type.md)

# sgcl::set\<Key, Hash, KeyEqual\>::node_type::node_type

```cpp
node_type() noexcept = default;           // (1)
node_type(node_type&& other) noexcept;    // (2)
```

Constructs a node handle.

1. An empty handle, holding no node.
2. Takes the node of `other` over; `other` is left empty.

A handle is not copyable: a handle that holds a node is made by [extract](../set/extract.md) of a set or a
multiset.

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
    set<string>::node_type empty;
    println("{}", empty.empty());

    set<string> s = {"a"};
    auto nh = s.extract("a");
    set<string>::node_type moved(std::move(nh));
    println("{} {} {}", nh.empty(), moved.empty(), moved.value());
    println("{}", std::is_copy_constructible_v<set<string>::node_type>);
}
```

Output:

```text
true
true false a
false
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [extract](../set/extract.md): a handle holding a node of a set
- [sgcl::set\<Key, Hash, KeyEqual\>::node_type](../set-node_type.md)
