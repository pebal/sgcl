[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md) › [node_type](../ordered_map-node_type.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type::swap

```cpp
void swap(node_type& other) noexcept;                         // (1)
friend void swap(node_type& lhs, node_type& rhs) noexcept;    // (2)
```

1. Exchanges the nodes of this handle and `other`, either of them possibly empty.
2. `lhs.swap(rhs)`: the non-member `swap`, a hidden friend found by the arguments' type.

No element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle to exchange the nodes with |
| `lhs`, `rhs` | the handles to exchange the nodes of |

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
    ordered_map<int, string> m = {{1, "one"}, {2, "two"}};
    auto a = m.extract(1);
    auto b = m.extract(2);
    swap(a, b);
    println("{} {}", a.mapped(), b.mapped());

    decltype(a) none;
    a.swap(none);
    println("{} {}", a.empty(), none.key());
}
```

Output:

```text
two one
true 2
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type](../ordered_map-node_type.md)
