[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](../weak_multimap.md)

# sgcl::weak_multimap\<Key, T\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last entry. A walk from [begin](begin.md) reaches it after the last live entry, and
[find](find.md) returns it for an object without an entry. The end of an [equal_range](equal_range.md) is another
iterator: the entry after the object's run, or this one.

## Parameters

None.

## Return value

The iterator past the last entry. It may not be dereferenced.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    weak_multimap<Node, int> scores;
    println("{}", scores.begin() == scores.end());

    tracked_ptr node = make_tracked<Node>(1);
    scores.insert(node, 10);
    println("{}", scores.cbegin() == scores.cend());

    tracked_ptr other = make_tracked<Node>(2);
    println("{}", scores.find(other) == scores.end());
}
```

Output:

```text
true
false
true
```

## See also

- [begin, cbegin](begin.md): an iterator to the first live entry
- [sgcl::weak_multimap\<Key, T\>](../weak_multimap.md)
