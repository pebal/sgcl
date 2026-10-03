[sgcl](../../README.md) › [immutable](../README.md) › [set](../set/README.md) › [builder](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements the builder holds now.

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
    auto b = s.thaw();
    b.insert(3);
    b.insert(1);
    println("{} {}", b.size(), s.size());
}
```

Output:

```text
3 2
```

## See also

- [empty](empty.md): checks whether the builder is empty
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](README.md)
