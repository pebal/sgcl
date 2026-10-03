[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map/README.md) › [node_type](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type::key

```cpp
key_type& key() const noexcept;
```

Returns a reference to the key of the element. It is writable: the node is out of any map, so a key changed
here is hashed afresh when the node is inserted. This is the way to change the key of an element without
copying it or its value. The handle must not be empty; nothing is checked.

## Parameters

None.

## Return value

A reference to the key.

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
    ordered_map<string, int> m = {{"old", 1}, {"other", 2}};
    auto nh = m.extract("old");
    nh.key() = "new";
    m.insert(std::move(nh));
    println("{}", m);
}
```

Output:

```text
{"other": 2, "new": 1}
```

## See also

- [mapped](mapped.md): the value of the element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type](README.md)
