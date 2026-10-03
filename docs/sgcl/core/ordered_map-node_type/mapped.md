[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md) › [node_type](../ordered_map-node_type.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type::mapped

```cpp
mapped_type& mapped() const noexcept;
```

Returns a reference to the value of the element, writable. The handle must not be empty; nothing is checked.

## Parameters

None.

## Return value

A reference to the value.

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
    ordered_map<string, vector<int>> m = {{"a", {1, 2}}};
    auto nh = m.extract("a");
    nh.mapped().push_back(3);  // the vector stays in the node
    m.insert(std::move(nh));
    println("{}", m);
}
```

Output:

```text
{"a": [1, 2, 3]}
```

## See also

- [key](key.md): the key of the element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type](../ordered_map-node_type.md)
