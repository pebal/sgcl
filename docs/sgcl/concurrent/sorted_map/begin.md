[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](README.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element in key order: the first node of the bottom list that is not erased. When
the map is empty, the iterator is equal to [end()](end.md).

The walk from it is weakly consistent: `++` steps to the next node of the bottom list that is not erased, so the
iteration sees the elements in key order, skips the ones erased since it passed them and may or may not see the
ones inserted meanwhile.

## Parameters

None.

## Return value

An iterator to the first element, or `end()` when there is none.

## Complexity

Constant, plus the erased nodes at the front that no search has unlinked yet. Each `++` the same.

## Exceptions

None.

## Notes

The iterator holds its node by a `tracked_ptr`: it is valid whatever the other threads do, and the element it
addresses lives for as long as the iterator does, erased or not. `begin` and `++` read the links and write
nothing; no thread waits for another. Through an `iterator` (1) the mapped value may be written; when other
threads read or write it at the same time, the synchronization is the program's.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> numbers = {{2, "two"}, {1, "one"}, {3, "three"}};

    for (auto& [key, name] : numbers) {
        println("{} {}", key, name);
    }

    auto first = numbers.cbegin();
    numbers.erase(1);  // the element lives while `first` holds its node
    println("{} {}", first->first, first->second);
    ++first;
    println("{}", first->second);
}
```

Output:

```text
1 one
2 two
3 three
1 one
two
```

## See also

- [end, cend](end.md): the iterator past the last element
- [lower_bound](lower_bound.md): an iterator to the first element from a key on
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](README.md)
