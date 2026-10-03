[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set/README.md) › [node_type](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type::empty

```cpp
bool empty() const noexcept;
```

Checks whether the handle holds no node: a default-constructed handle, one moved from, one whose node was
inserted, the result of an [extract](../ordered_set/extract.md) of an element that is not there.

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
    ordered_set<int> s = {1};
    auto nh = s.extract(1);
    println("{} {}", nh.empty(), s.extract(2).empty());

    s.insert(std::move(nh));
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
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type](README.md)
