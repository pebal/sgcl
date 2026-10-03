[sgcl](../../README.md) › [concurrent](../README.md) › [set](README.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element of the list: a walk from the head over the dummies of the buckets and
the erased nodes to the first element that is there. The keys come in the order of the list, the bit reversal of
their hashes, which depends on the keys alone, not on the order of the insertions. An empty set gives
[end()](end.md). `iterator` and `const_iterator` are one type: the keys are const.

## Parameters

None.

## Return value

An iterator to the first element, or `end()` when the walk found none.

## Complexity

Constant, plus the dummies and the erased nodes before the first element: after a [clear](clear.md) of a large
set, up to one step per bucket used.

## Exceptions

None.

## Notes

Lock-free, and it writes nothing to the set. The iterator holds its node by a `tracked_ptr`: it is valid whatever
the other threads do, and an increment walks the list from its node as `begin` walks it from the head. Iteration
is weakly consistent: a pass skips the elements erased before it reaches them and may or may not see the ones
inserted meanwhile.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8};

    vector<int> keys;
    for (auto it = numbers.cbegin(); it != numbers.cend(); ++it) {
        keys.push_back(*it);
    }
    println("{}", keys);
}
```

Sample output:

```text
[8, 4, 2, 6, 1, 5, 3, 7]
```

## See also

- [end, cend](end.md): the iterator past the last element
- [find](find.md): an iterator to the element equal to a key
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](README.md)
