[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map/README.md) › [node_type](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type::mapped

```cpp
mapped_type& mapped() const noexcept;
```

Returns a reference to the mapped value of the element the handle owns. The handle must not be empty.

## Parameters

None.

## Return value

A reference to the mapped value of the element.

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
    sorted_map<int, string> m = {{1, "one"}};
    auto nh = m.extract(1);
    nh.mapped() = "uno";
    string taken = std::move(nh.mapped());  // the value moved out of the node
    println("{} {}", taken, m.empty());
}
```

Output:

```text
uno true
```

## See also

- [key](key.md): the key of the element
- [sgcl::sorted_map\<Key, T, Compare\>::node_type](README.md)
