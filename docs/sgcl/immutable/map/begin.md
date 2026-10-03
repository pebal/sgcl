[sgcl](../../README.md) › [immutable](../README.md) › [map](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::begin, cbegin

```cpp
const_iterator begin() const noexcept;     // (1)
const_iterator cbegin() const noexcept;    // (2)
```

Returns an iterator to the first element in the order of the trie; on an empty map it is equal to
[end()](end.md).

- (1–2) The same iterator: every iterator of the map is a `const_iterator`.

The iterator is a forward iterator over `pair<const Key, T>`. It holds the path from the root, a node and the
slots still to visit per level, and the element of a chain when it is in one: `++` goes to the next slot, down
into a subtrie, or up when a node is done. The order is the bits of the hashes, the low five first; it changes
with nothing but the elements.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Logarithmic in `size()`, base 32: the walk down to the first element.

## Exceptions

None.

## Notes

The iterator keeps nothing alive: it is valid while this map object exists and holds the version it was taken
from. A walk of a hundred thousand elements costs 8.5 ns per element.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> stock = {{"apples", 3}, {"pears", 0}, {"plums", 12}};
    int total = 0;
    for (const auto& [item, count] : stock) {
        total += count;
    }
    println("{} items in {} kinds", total, stock.size());
    println("{}", stock.cbegin() == stock.begin());
}
```

Output:

```text
15 items in 3 kinds
true
```

## See also

- [end, cend](end.md): an iterator to the end
- [find](find.md): an iterator to the element under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](README.md)
