[sgcl](../../README.md) › [immutable](../README.md) › [set](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, a word of the set.

## Parameters

None.

## Return value

The number of elements.

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
    immutable::set<int> s = {1, 2};
    println("{} {} {}", s.size(), s.insert(2).size(), s.insert(3).size());
}
```

Output:

```text
2 2 3
```

## See also

- [empty](empty.md): checks whether the set is empty
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](README.md)
