[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](../weak_map.md)

# sgcl::concurrent::weak_map\<Key, T\>::end

```cpp
iterator end() noexcept;
```

Returns the iterator past the last entry: the end of the table's list, which holds nothing. A walk from
[begin](begin.md) reaches it after the last live entry, and [find](find.md) returns it for an object without an
entry.

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
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    concurrent::weak_map<Node, int> ranks;
    println("{}", ranks.begin() == ranks.end());

    tracked_ptr node = make_tracked<Node>(1);
    ranks.try_emplace(node, 10);
    println("{}", ranks.begin() == ranks.end());

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

- [begin](begin.md): an iterator to the first live entry
- [sgcl::concurrent::weak_map\<Key, T\>](../weak_map.md)
