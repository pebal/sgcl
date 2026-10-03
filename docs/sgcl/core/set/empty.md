[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the set holds no element: whether its stored count is 0.

## Parameters

None.

## Return value

`true` when the set is empty, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

An empty set may still have buckets: [clear](clear.md) and the erasures keep them.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<string> s;
    println("{}", s.empty());

    s.insert("a");
    println("{}", s.empty());

    s.clear();
    println("{} {}", s.empty(), s.bucket_count());
}
```

Output:

```text
true
false
true 8
```

## See also

- [size](size.md): the number of elements
- [clear](clear.md): destroys every element
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)
