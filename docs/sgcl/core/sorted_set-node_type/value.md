[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md) › [node_type](../sorted_set-node_type.md)

# sgcl::sorted_set\<Key, Compare\>::node_type::value

```cpp
value_type& value() const noexcept;
```

Returns a reference to the element in the node. It is writable: in a set the element is the key, which may not
change while the node is in a tree, and may while the handle holds it. The handle must not be empty.

## Parameters

None.

## Return value

A reference to the element.

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
    sorted_set<int> ids = {10, 20, 30};

    auto nh = ids.extract(10);
    nh.value() = 40;  // a new key, and the node goes to its new place
    ids.insert(std::move(nh));
    println("{}", ids);
}
```

Output:

```text
{20, 30, 40}
```

## See also

- [extract](../sorted_set/extract.md): takes a node out of a set
- [sgcl::sorted_set\<Key, Compare\>::node_type](../sorted_set-node_type.md)
