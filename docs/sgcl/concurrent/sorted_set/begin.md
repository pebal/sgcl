[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::begin, cbegin

```cpp
/*(1)*/ iterator begin() noexcept;
/*(2)*/ const_iterator begin() const noexcept;
/*(3)*/ const_iterator cbegin() const noexcept;
```

Returns an iterator to the first key in order: the first node of the bottom list that is not erased. When the set
is empty, the iterator is equal to [end()](end.md). `iterator` and `const_iterator` are one type, over `const Key`.

The walk from it is weakly consistent: `++` steps to the next node of the bottom list that is not erased, so the
iteration sees the keys in order, skips the ones erased since it passed them and may or may not see the ones
inserted meanwhile.

## Parameters

None.

## Return value

An iterator to the first key, or `end()` when there is none.

## Complexity

Constant, plus the erased nodes at the front that no search has unlinked yet. Each `++` the same.

## Exceptions

None.

## Notes

The iterator holds its node by a `tracked_ptr`: it is valid whatever the other threads do, and the key it
addresses lives for as long as the iterator does, erased or not. `begin` and `++` read the links and write
nothing; no thread waits for another.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<string> tags = {"red", "blue", "green"};

    for (const string& tag : tags) {
        println("{}", tag);
    }

    auto first = tags.begin();
    tags.erase("blue");  // the key lives while `first` holds its node
    println("{}", *first);
    ++first;
    println("{}", *first);
}
```

Output:

```text
blue
green
red
blue
green
```

## See also

- [end, cend](end.md): the iterator past the last key
- [lower_bound](lower_bound.md): an iterator to the first key from a key on
- [sgcl::concurrent::sorted_set\<Key, Compare\>](../sorted_set.md)
