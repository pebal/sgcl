[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md) › [node_type](../sorted_set-node_type.md)

# sgcl::sorted_set\<Key, Compare\>::node_type::empty

```cpp
[[nodiscard]] bool empty() const noexcept;
```

Checks whether the handle owns no node: a handle constructed empty, moved from, inserted from, or returned by
[extract](../sorted_set/extract.md) of a key that is not there.

## Parameters

None.

## Return value

`true` when the handle owns no node, `false` otherwise.

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
    sorted_set<string> names = {"ada"};

    println("{}", names.extract("bob").empty());

    auto nh = names.extract("ada");
    println("{}", nh.empty());

    names.insert(std::move(nh));
    println("{}", nh.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [operator bool](operator_bool.md): the same question the other way
- [sgcl::sorted_set\<Key, Compare\>::node_type](../sorted_set-node_type.md)
