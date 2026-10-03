[sgcl](../../README.md) › [immutable](../README.md) › [map](../map/README.md) › [builder](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::size

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
    immutable::map<int, int> m = {{1, 10}, {2, 20}};
    auto b = m.thaw();
    b.insert(3, 30);
    b.erase(1);
    b.set(2, 21);
    println("{} {}", b.size(), m.size());
}
```

Output:

```text
2 2
```

## See also

- [empty](empty.md): checks whether the builder is empty
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](README.md)
