[sgcl](../../README.md) › [core](../README.md) › [weak_map](README.md)

# sgcl::weak_map\<Key, T\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last entry. A walk from [begin](begin.md) reaches it after the last live entry, and
[find](find.md) returns it for an object without an entry.

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
    weak_map<Node, int> ranks;
    println("{}", ranks.begin() == ranks.end());

    tracked_ptr node = make_tracked<Node>(1);
    ranks[node] = 10;
    println("{}", ranks.cbegin() == ranks.cend());

    tracked_ptr other = make_tracked<Node>(2);
    println("{}", ranks.find(other) == ranks.end());
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
- [sgcl::weak_map\<Key, T\>](README.md)
