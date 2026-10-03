[sgcl](../../README.md) › [core](../README.md) › [map](../map.md) › [node_type](../map-node_type.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::node_type::operator=

```cpp
node_type& operator=(node_type&& other) noexcept;
```

Takes the node of `other` over; `other` is empty after. The element of the node this handle held before, if any,
is destroyed first. Assigning a handle to itself does nothing. There is no copy assignment.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose node to take |

## Return value

`*this`.

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
    map<int, string> m = {{1, "one"}, {2, "two"}};
    auto nh = m.extract(1);
    nh = m.extract(2);  // "one" is destroyed here
    println("{} {} {}", nh.key(), nh.mapped(), m.empty());

    map<int, string>::node_type other;
    other = std::move(nh);
    println("{} {}", nh.empty(), other.key());
}
```

Output:

```text
2 two true
true 2
```

## See also

- [(constructor)](map-node_type.md): constructs a node handle
- [swap](swap.md): swaps the nodes of two handles
- [sgcl::map\<Key, T, Hash, KeyEqual\>::node_type](../map-node_type.md)
