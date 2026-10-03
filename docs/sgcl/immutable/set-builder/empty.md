[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md) › [builder](../set-builder.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::empty

```cpp
bool empty() const noexcept;
```

Checks whether the builder holds no elements: a new one, one moved from, or one whose every element was erased.

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
    immutable::set<int> s = {1};
    auto b = s.thaw();
    bool at_first = b.empty();
    b.erase(1);
    println("{} {} {}", at_first, b.empty(), s.empty());
}
```

Output:

```text
false true false
```

## See also

- [size](size.md): the number of elements
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](../set-builder.md)
