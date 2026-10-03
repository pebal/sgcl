[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the smallest element, the first in the order of `Compare`. The tree keeps its leftmost node
in its header, so nothing is searched. An empty multiset gives [end()](end.md). Equivalent keys follow one another
in the order of their insertion.

`iterator` and `const_iterator` are one type, over `const Key`: a key is never changed in place, where the order
depends on it.

## Parameters

None.

## Return value

An iterator to the first element, or `end()` when the multiset is empty.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator is one raw pointer to its node: copying it and stepping it read the links and pay no write barrier, and
it may be kept anywhere, a `std::vector` of iterators included, while its element is in the multiset. Before the
first insertion the multiset has no header node yet, and `begin()` and `end()` are both null iterators, equal to
each other.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multiset<string> words = {"b", "a", "a"};

    println("{}", *words.begin());

    vector<string> in_order;
    for (auto it = words.cbegin(); it != words.cend(); ++it) {
        in_order.push_back(*it);
    }
    println("{}", in_order);

    sorted_multiset<string> empty;
    println("{}", empty.begin() == empty.end());
}
```

Output:

```text
a
["a", "a", "b"]
true
```

## See also

- [end, cend](end.md): the iterator past the last element
- [rbegin, crbegin](rbegin.md): the order from the largest element
- [min](min.md): the smallest element itself
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
