[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md) › [node_type](../ordered_map-node_type.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type::operator=

```cpp
node_type& operator=(node_type&& other) noexcept;
```

Destroys the element of the node this handle holds, if any, and takes the node of `other` over; `other` is empty
after. Assigning a handle to itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle the node is taken from |

## Return value

`*this`.

## Complexity

Constant, plus the destructor of the element this handle held.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, string> m = {{1, "one"}, {2, "two"}};
    auto nh = m.extract(1);
    nh = m.extract(2);  // "one" is destroyed here
    println("{} {} {}", nh.key(), nh.mapped(), m.empty());
}
```

Output:

```text
2 two true
```

## See also

- [(constructor)](ordered_map-node_type.md): constructs a handle
- [swap](swap.md): exchanges the nodes of two handles
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type](../ordered_map-node_type.md)
