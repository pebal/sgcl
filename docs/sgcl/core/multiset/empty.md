[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the multiset holds no element: whether its stored count is 0.

## Parameters

None.

## Return value

`true` when the multiset is empty, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

An empty multiset may still have buckets: [clear](clear.md) and the erasures keep them.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<string> s;
    println("{}", s.empty());

    s.insert("a");
    s.insert("a");
    println("{}", s.empty());

    s.erase("a");
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
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
