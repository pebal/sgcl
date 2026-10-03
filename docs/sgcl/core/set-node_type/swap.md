[sgcl](../../README.md) › [core](../README.md) › [set](../set.md) › [node_type](../set-node_type.md)

# sgcl::set\<Key, Hash, KeyEqual\>::node_type::swap

```cpp
void swap(node_type& other) noexcept;                         // (1)
friend void swap(node_type& lhs, node_type& rhs) noexcept;    // (2)
```

Exchanges the nodes of two handles, either of them possibly empty; no element is touched.

1. Exchanges the nodes of this handle and `other`.
2. Exchanges the nodes of `lhs` and `rhs`, as `lhs.swap(rhs)`: a hidden friend, found by the arguments' type.

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
    set<string> s = {"a", "b"};
    auto first = s.extract("a");
    auto second = s.extract("b");
    swap(first, second);
    println("{} {}", first.value(), second.value());

    set<string>::node_type empty;
    first.swap(empty);
    println("{} {}", first.empty(), empty.value());
}
```

Output:

```text
b a
true b
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [sgcl::set\<Key, Hash, KeyEqual\>::node_type](../set-node_type.md)
