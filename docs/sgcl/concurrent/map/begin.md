[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element of the list: a walk from the head over the dummies of the buckets and
the erased nodes to the first element that is there. The elements come in the order of the list, the bit reversal
of their hashes, which depends on the keys alone, not on the order of the insertions. An empty map gives
[end()](end.md).

## Parameters

None.

## Return value

An iterator to the first element, or `end()` when the walk found none.

## Complexity

Constant, plus the dummies and the erased nodes before the first element: after a [clear](clear.md) of a large
map, up to one step per bucket used.

## Exceptions

None.

## Notes

Lock-free, and it writes nothing to the map. The iterator holds its node by a `tracked_ptr`: it is valid whatever
the other threads do, and an increment walks the list from its node as `begin` walks it from the head. Iteration
is weakly consistent: a pass skips the elements erased before it reaches them and may or may not see the ones
inserted meanwhile.

Through an `iterator` the value of an element can be changed in place; the map does not synchronize that write
with the readers of the element ([Rules](../map.md#rules)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<int, int> squares;
    for (int i : range(1, 9)) {
        squares.try_emplace(i, i * i);
    }

    vector<int> keys;
    int sum = 0;
    for (auto it = squares.cbegin(); it != squares.cend(); ++it) {
        keys.push_back(it->first);
        sum += it->second;
    }
    println("{} {}", keys, sum);
}
```

Sample output:

```text
[8, 4, 2, 6, 1, 5, 3, 7] 204
```

## See also

- [end, cend](end.md): the iterator past the last element
- [find](find.md): an iterator to the element under a key
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
