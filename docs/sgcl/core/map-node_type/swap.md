[sgcl](../../README.md) › [core](../README.md) › [map](../map.md) › [node_type](../map-node_type.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::node_type::swap

```cpp
void swap(node_type& other) noexcept;                         // (1)
friend void swap(node_type& lhs, node_type& rhs) noexcept;    // (2)
```

Exchanges the nodes of two handles; no element is touched.

1. Exchanges the nodes of this handle and `other`.
2. `lhs.swap(rhs)`, as a non-member found by argument-dependent lookup.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle to exchange the node with |
| `lhs`, `rhs` | the handles whose nodes to exchange |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, string> m = {{1, "one"}};
    auto a = m.extract(1);
    map<int, string>::node_type b;

    swap(a, b);
    println("{} {}", a.empty(), b.mapped());

    a.swap(b);
    println("{} {}", a.key(), b.empty());
}
```

Output:

```text
true one
1 true
```

## See also

- [operator=](operator_assign.md): takes another handle's node over
- [sgcl::map\<Key, T, Hash, KeyEqual\>::node_type](../map-node_type.md)
