[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md) › [node_type](../ordered_set-node_type.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type::swap

```cpp
/*(1)*/ void swap(node_type& other) noexcept;
/*(2)*/ friend void swap(node_type& lhs, node_type& rhs) noexcept;
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
    ordered_set<int> s = {1, 2};
    auto a = s.extract(1);
    auto b = s.extract(2);
    swap(a, b);
    println("{} {}", a.value(), b.value());

    decltype(a) none;
    a.swap(none);
    println("{} {}", a.empty(), none.value());
}
```

Output:

```text
2 1
true 2
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type](../ordered_set-node_type.md)
