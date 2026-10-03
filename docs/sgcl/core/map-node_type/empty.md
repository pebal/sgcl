[sgcl](../../README.md) › [core](../README.md) › [map](../map.md) › [node_type](../map-node_type.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::node_type::empty

```cpp
bool empty() const noexcept;
```

Checks whether the handle holds no node: a handle constructed empty, moved from, inserted from, or returned by
an [extract](../map/extract.md) of an absent key.

## Parameters

None.

## Return value

`true` when the handle holds no node, `false` otherwise.

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
    map<int, string> m = {{1, "one"}};
    auto absent = m.extract(7);
    auto found = m.extract(1);
    println("{} {}", absent.empty(), found.empty());

    m.insert(std::move(found));
    println("{}", found.empty());
}
```

Output:

```text
true false
true
```

## See also

- [operator bool](operator_bool.md): the opposite answer, in a condition
- [sgcl::map\<Key, T, Hash, KeyEqual\>::node_type](../map-node_type.md)
