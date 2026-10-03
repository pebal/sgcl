[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last element: the header node, which may not be dereferenced. `--end()` is the
element with the largest key.

Before the first insertion there is no header: `end()` and [begin()](begin.md) are both null iterators, equal to
each other, which may be neither dereferenced nor moved, and an `end()` taken then does not compare equal to
`end()` after the first insertion. From then on `end()` stays the same: [clear](clear.md) keeps the header.

## Parameters

None.

## Return value

The iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    sorted_map<int, string> m;
    auto before = m.end();
    println("{}", m.begin() == m.end());

    m = {{1, "a"}, {3, "c"}, {2, "b"}};
    auto last = std::prev(m.end());
    println("{} {}", last->second, before == m.end());

    auto after = m.cend();
    m.clear();
    println("{}", after == m.end());
}
```

Output:

```text
true
c false
true
```

## See also

- [begin, cbegin](begin.md): an iterator to the first element
- [rend, crend](rend.md): the reverse iterator past the first element
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
