[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element; on an empty vector it is equal to [end()](end.md).

- (1) An iterator through which the elements may be changed.
- (2–3) A `const_iterator`: `cbegin` gives it on a vector that is not `const` too.

The iterator is a plain pointer to the element in a class of the library, a `std::contiguous_iterator`: the
algorithms of `<algorithm>` and `std::ranges` apply, and `std::to_address(it)` is the address of the element.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

The iterator keeps nothing alive: like a pointer to an element, it is valid as long as with `std::vector`
([Iterator invalidation](README.md#iterator-invalidation)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>
#include <numeric>

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 1};
    std::sort(v.begin(), v.end());
    int sum = std::accumulate(v.cbegin(), v.cend(), 0);
    println("{} sum {}", v, sum);

    auto it = std::ranges::find(v, 5);
    println("5 at {}", it - v.begin());
}
```

Output:

```text
[1, 3, 5, 9] sum 18
5 at 2
```

## See also

- [end, cend](end.md): an iterator to the end
- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [sgcl::vector\<T\>](README.md)
