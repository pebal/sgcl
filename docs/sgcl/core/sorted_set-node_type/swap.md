[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md) › [node_type](../sorted_set-node_type.md)

# sgcl::sorted_set\<Key, Compare\>::node_type::swap

```cpp
/*(1)*/ void swap(node_type& other) noexcept;
/*(2)*/ friend void swap(node_type& lhs, node_type& rhs) noexcept;
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
    sorted_set<string> s = {"ash", "oak"};
    auto a = s.extract("ash");
    auto b = s.extract("oak");

    swap(a, b);
    println("{} {}", a.value(), b.value());

    b.swap(a);
    println("{} {}", a.value(), b.value());
}
```

Output:

```text
oak ash
ash oak
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [sgcl::sorted_set\<Key, Compare\>::node_type](../sorted_set-node_type.md)
