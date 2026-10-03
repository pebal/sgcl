[sgcl](../../README.md) › [immutable](../README.md) › [map](../map/README.md) › [builder](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::empty

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
    immutable::map<int, int>::builder b;
    bool at_first = b.empty();
    b.insert(1, 1);
    bool after_insert = b.empty();
    b.erase(1);
    println("{} {} {}", at_first, after_insert, b.empty());
}
```

Output:

```text
true false true
```

## See also

- [size](size.md): the number of elements
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](README.md)
