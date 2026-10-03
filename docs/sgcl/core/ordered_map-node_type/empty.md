[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map/README.md) › [node_type](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type::empty

```cpp
bool empty() const noexcept;
```

Checks whether the handle holds no node: a default-constructed handle, one moved from, one whose node was
inserted, the result of an [extract](../ordered_map/extract.md) of a key that is absent.

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
    ordered_map<int, string> m = {{1, "one"}};
    auto nh = m.extract(1);
    println("{} {}", nh.empty(), m.extract(2).empty());

    m.insert(std::move(nh));
    println("{}", nh.empty());
}
```

Output:

```text
false true
true
```

## See also

- [operator bool](operator_bool.md): checks whether the handle holds a node
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type](README.md)
