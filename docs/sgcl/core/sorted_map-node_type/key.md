[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md) › [node_type](../sorted_map-node_type.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type::key

```cpp
key_type& key() const noexcept;
```

Returns a reference to the key of the element the handle owns. It is writable, as in `std`: the node is out of
any map, so its key may change before it goes back in, which no map allows for an element it holds. The handle
must not be empty.

## Parameters

None.

## Return value

A reference to the key of the element.

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
    sorted_map<string, int> m = {{"draft", 1}, {"final", 2}};
    auto nh = m.extract("draft");
    nh.key() = "published";  // renamed without copying the value
    m.insert(std::move(nh));
    println("{}", m);
}
```

Output:

```text
{"final": 2, "published": 1}
```

## See also

- [mapped](mapped.md): the mapped value of the element
- [sgcl::sorted_map\<Key, T, Compare\>::node_type](../sorted_map-node_type.md)
