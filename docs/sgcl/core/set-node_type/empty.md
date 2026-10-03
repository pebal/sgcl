[sgcl](../../README.md) › [core](../README.md) › [set](../set.md) › [node_type](../set-node_type.md)

# sgcl::set\<Key, Hash, KeyEqual\>::node_type::empty

```cpp
bool empty() const noexcept;
```

Checks whether the handle holds no node: a default-constructed handle, one moved from or inserted, or one that
[extract](../set/extract.md) of a key that is not there returned.

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
    set<string> s = {"a"};
    auto found = s.extract("a");
    auto missing = s.extract("z");
    println("{} {}", found.empty(), missing.empty());

    s.insert(std::move(found));
    println("{}", found.empty());
}
```

Output:

```text
false true
true
```

## See also

- [operator bool](operator_bool.md): the opposite question
- [sgcl::set\<Key, Hash, KeyEqual\>::node_type](../set-node_type.md)
