[sgcl](../../README.md) › [core](../README.md) › [map](../map/README.md) › [node_type](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::node_type::mapped

```cpp
mapped_type& mapped() const noexcept;
```

Returns a reference to the value of the element the handle holds, writable. The handle must not be empty.

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
    map<int, vector<int>> m = {{1, {1, 2}}};
    auto nh = m.extract(1);
    nh.mapped().push_back(3);
    m.insert(std::move(nh));
    println("{}", m.at(1));
}
```

Output:

```text
[1, 2, 3]
```

## See also

- [key](key.md): the key of the element
- [sgcl::map\<Key, T, Hash, KeyEqual\>::node_type](README.md)
