[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::begin, cbegin

```cpp
/*(1)*/ iterator begin() noexcept;
/*(2)*/ const_iterator begin() const noexcept;
/*(3)*/ const_iterator cbegin() const noexcept;
```

Returns an iterator to the first element; on an empty deque, an iterator equal to [end](end.md).

- (1) An iterator through which the elements may be written.
- (2–3) A `const_iterator`, through which they are only read.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

The iterators are random-access: a step within a block and the access are plain loads, a step across a block
boundary one load of the map, so `std::ranges` algorithms and `std::sort` work on the deque. An iterator does not
keep its element valid; it is invalidated exactly when a `std::deque` iterator is: by any insertion, and by an
erasure of its element or in the middle ([Iterator invalidation](../deque.md#iterator-invalidation)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

int main() {
    deque d = {3, 1, 2};
    std::ranges::sort(d);
    println("{}", d);

    for (auto it = d.begin(); it != d.end(); ++it) {
        *it *= 10;
    }
    println("{} {}", d, *(d.cbegin() + 2));
}
```

Output:

```text
[1, 2, 3]
[10, 20, 30] 30
```

## See also

- [end, cend](end.md): an iterator to the end
- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [sgcl::deque\<T\>](../deque.md)
