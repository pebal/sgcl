[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::begin, cbegin

```cpp
/*(1)*/ iterator begin() const noexcept;
/*(2)*/ const_iterator cbegin() const noexcept;
```

An iterator to the first element: a plain pointer, `T*` (1) or `const T*` (2). Equal to [end](end.md) for an empty
slice.

## Parameters

None.

## Return value

An iterator to the beginning.

## Complexity

Constant.

## Exceptions

None.

## Notes

The iterator is a raw pointer and keeps nothing alive: it is valid while the slice, or another holder of the owner,
lives.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <numeric>

using namespace sgcl;

int main() {
    vector v = {1, 2, 3, 4};
    slice<const int> s = v.as_slice(1);
    println("{} {}", *s.begin(), std::accumulate(s.cbegin(), s.cend(), 0));
}
```

Output:

```text
2 9
```

## See also

- [end, cend](end.md): an iterator to the end
- [rbegin, crbegin](rbegin.md): a reverse iterator
- [sgcl::slice\<T\>](../slice.md)
