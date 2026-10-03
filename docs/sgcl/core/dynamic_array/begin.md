[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element; on an empty array it is equal to [end()](end.md).

- (1) An iterator that writes the elements.
- (2–3) An iterator that reads them.

The iterator is a raw pointer in a thin class, a `std::contiguous_iterator`: cheap to copy, at home in any
container, and the algorithms of `<algorithm>` and `std::ranges` apply.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator keeps nothing alive. One that dies in a frame nulls its word, so that a temporary left behind does
not root the buffer under the collector's conservative scan of the stack. The buffer never moves: an iterator is
valid until the array is assigned over, moved from or destroyed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>
#include <numeric>

using namespace sgcl;

int main() {
    dynamic_array<int> a = {5, 3, 9, 1};
    auto first = a.begin();
    std::ranges::sort(a);
    println("{} {} {}", a, *first, std::accumulate(a.cbegin(), a.cend(), 0));
}
```

Output:

```text
[1, 3, 5, 9] 1 18
```

## See also

- [end, cend](end.md): an iterator to the end
- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
