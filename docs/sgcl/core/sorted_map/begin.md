[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::begin, cbegin

```cpp
/*(1)*/ iterator begin() noexcept;
/*(2)*/ const_iterator begin() const noexcept;
/*(3)*/ const_iterator cbegin() const noexcept;
```

Returns an iterator to the first element, the one with the smallest key. From it, `++` walks the elements in the
order of `Compare`. When the map is empty, the iterator is equal to [end()](end.md); before the first insertion
both are null iterators, which may be neither dereferenced nor moved.

An iterator is one raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be
kept in unmanaged memory (a `std::vector<iterator>`) for as long as its element is in the map. Through an
`iterator` (1) the mapped value may be written; the key is `const`.

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
#include <vector>

using namespace sgcl;

int main() {
    sorted_map<string, int> m = {{"b", 2}, {"a", 1}, {"c", 3}};
    for (auto& [key, value] : m) {
        value *= 10;
    }
    println("{}", m);

    std::vector<sorted_map<string, int>::iterator> kept;  // unmanaged memory: fine
    kept.push_back(m.begin());
    m.insert({"0", 0});
    println("{} {}", kept[0]->first, m.cbegin()->first);
}
```

Output:

```text
{"a": 10, "b": 20, "c": 30}
a 0
```

## See also

- [end, cend](end.md): the iterator past the last element
- [rbegin, crbegin](rbegin.md): a reverse iterator to the last element
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
