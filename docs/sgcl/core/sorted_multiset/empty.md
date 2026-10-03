[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::empty

```cpp
[[nodiscard]] bool empty() const noexcept;
```

Checks whether the multiset holds no element.

## Parameters

None.

## Return value

`true` when the multiset is empty, `false` otherwise.

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
    sorted_multiset<int> seen;
    println("{}", seen.empty());

    seen.insert({7, 7});
    println("{}", seen.empty());

    seen.erase(7);  // both
    println("{}", seen.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements
- [clear](clear.md): destroys every element
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
