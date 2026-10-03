[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md) › [node_type](../sorted_set-node_type.md)

# sgcl::sorted_set\<Key, Compare\>::node_type::node_type

```cpp
node_type() noexcept = default;           // (1)
node_type(node_type&& other) noexcept;    // (2)
```

Constructs a node handle.

1. An empty handle, owning no node. The handles worth having come from [extract](../sorted_set/extract.md) and
   from the `node` of an [insert](../sorted_set/insert.md) whose key was taken.
2. Takes the node of `other`, which is left empty.

A handle is not copyable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle to take the node from |

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
    sorted_set<int>::node_type empty;
    println("{}", empty.empty());

    sorted_set<int> numbers = {1, 2};
    auto nh = numbers.extract(1);
    sorted_set<int>::node_type taken(std::move(nh));
    println("{} {} {}", taken.value(), nh.empty(), numbers);

    println("{}", std::is_copy_constructible_v<sorted_set<int>::node_type>);
}
```

Output:

```text
true
1 true {2}
false
```

## See also

- [operator=](operator_assign.md): takes the node of another handle
- [sgcl::sorted_set\<Key, Compare\>::node_type](../sorted_set-node_type.md)
