[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map/README.md) › [node_type](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type::swap

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
    sorted_map<int, string> m = {{1, "one"}, {2, "two"}};
    auto a = m.extract(1);
    sorted_map<int, string>::node_type b;

    swap(a, b);
    println("{} {}", a.empty(), b.mapped());

    a = m.extract(2);
    a.swap(b);
    println("{} {}", a.key(), b.key());
}
```

Output:

```text
true one
1 2
```

## See also

- [operator=](operator_assign.md): takes another handle's node
- [sgcl::sorted_map\<Key, T, Compare\>::node_type](README.md)
