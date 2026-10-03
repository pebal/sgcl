[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the largest element: the header node of the tree, which is not an element and may not be
dereferenced. Decremented, it gives the largest element, the last inserted of the largest equivalent keys.

## Parameters

None.

## Return value

The iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Notes

The header node is made on the first insertion. Before it, `end()` is a null iterator, equal to `begin()` but
neither decremented nor dereferenced, and an `end()` taken then does not compare equal to `end()` after the first
insertion. From the first insertion on, `end()` stays the same iterator for the life of the multiset, across
insertions, erasures and `clear`, and it follows the tree in a `swap` or a move.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    sorted_multiset<int> numbers;
    auto before = numbers.end();
    numbers.insert({4, 8, 8});

    println("{}", *std::prev(numbers.end()));
    println("{}", numbers.find(16) == numbers.cend());
    println("{}", before == numbers.end());
}
```

Output:

```text
8
true
false
```

## See also

- [begin, cbegin](begin.md): the iterator to the first element
- [max](max.md): the largest element itself
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
