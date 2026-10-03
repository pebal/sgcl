[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the set has no elements. An empty set holds no node at all, including one erased down to nothing.

## Parameters

None.

## Return value

`true` when `size() == 0`, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<int> s;
    auto one = s.insert(1);
    println("{} {} {}", s.empty(), one.empty(), one.erase(1).empty());
}
```

Output:

```text
true false true
```

## See also

- [size](size.md): the number of elements
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
