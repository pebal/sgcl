[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element, the first one with the smallest key. From it, `++` walks the elements
in the order of `Compare`, equivalent keys in the order they were inserted. When the multimap is empty, the
iterator is equal to [end()](end.md); before the first insertion both are null iterators, which may be neither
dereferenced nor moved.

An iterator is one raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be
kept in unmanaged memory for as long as its element is in the multimap. Through an `iterator` (1) the mapped value
may be written; the key is `const`.

## Parameters

None.

## Return value

An iterator to the first element, or `end()` when there is none.

## Complexity

Constant: the header keeps the leftmost node. Each `++` is amortized constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<string, int> m = {{"b", 2}, {"a", 1}, {"a", 3}};
    for (auto& [key, value] : m) {
        value *= 10;
    }
    println("{}", m);
    println("{}", m.cbegin()->second);
}
```

Output:

```text
{"a": 10, "a": 30, "b": 20}
10
```

## See also

- [end, cend](end.md): the iterator past the last element
- [rbegin, crbegin](rbegin.md): a reverse iterator to the last element
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
