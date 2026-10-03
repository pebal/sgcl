[sgcl](../../README.md) › [core](../README.md) › [map](../map.md) › [node_type](../map-node_type.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::node_type::key

```cpp
key_type& key() const noexcept;
```

Returns a reference to the key of the element the handle holds. The reference is writable: out of a map, the key
may change, and the map the node is inserted into next hashes it again. The handle must not be empty.

## Parameters

None.

## Return value

A reference to the key.

## Complexity

Constant.

## Exceptions

None.

## Notes

Changing a key in place, without the copy of an erase and an insertion, is what a node handle is for: the element
keeps its node, its address and its value.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> m = {{"draft", 3}};
    auto nh = m.extract("draft");
    nh.key() = "final";
    m.insert(std::move(nh));
    println("{} {}", m.at("final"), m.contains("draft"));
}
```

Output:

```text
3 false
```

## See also

- [mapped](mapped.md): the value of the element
- [insert](../map/insert.md): inserts a node handle's node
- [sgcl::map\<Key, T, Hash, KeyEqual\>::node_type](../map-node_type.md)
