[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map/README.md) › [node_type](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type::empty

```cpp
[[nodiscard]] bool empty() const noexcept;
```

Checks whether the handle owns no node: a default-constructed handle, one moved from, one whose node was
inserted, or the result of [extract](../sorted_map/extract.md) by a key the map does not hold.

## Parameters

None.

## Return value

`true` when the handle owns no node.

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
    auto absent = m.extract(5);
    auto nh = m.extract(1);
    println("{} {}", absent.empty(), nh.empty());

    m.insert(std::move(nh));
    println("{}", nh.empty());
}
```

Output:

```text
true false
true
```

## See also

- [operator bool](operator_bool.md): the same question, the other way round
- [sgcl::sorted_map\<Key, T, Compare\>::node_type](README.md)
