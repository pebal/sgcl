[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::begin, cbegin

```cpp
/*(1)*/ const_iterator begin() const noexcept;
/*(2)*/ const_iterator cbegin() const noexcept;
```

Returns an iterator to the first element in the order of the trie; on an empty set it is equal to
[end()](end.md).

- (1–2) The same iterator: every iterator of the set is a `const_iterator`.

The iterator is a forward iterator over `const Key`, which holds the path from the root as the
[map's](../map/begin.md) does. The order is the bits of the hashes; it changes with nothing but the elements.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Logarithmic in `size()`, base 32: the walk down to the first element.

## Exceptions

None.

## Notes

The iterator keeps nothing alive: it is valid while this set object exists and holds the version it was taken
from.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string> tags = {"red", "green", "blue"};
    size_t letters = 0;
    for (const string& tag : tags) {
        letters += tag.size();
    }
    println("{} letters, {}", letters, tags.cbegin() == tags.begin());
}
```

Output:

```text
12 letters, true
```

## See also

- [end, cend](end.md): an iterator to the end
- [find](find.md): an iterator to the element equal to a key
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
