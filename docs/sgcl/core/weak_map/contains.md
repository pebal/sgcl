[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::contains

```cpp
bool contains(const key_pointer& object) const noexcept;
```

Checks whether `object` has an entry: the search of [find](find.md), without an iterator. A null pointer has no
entry.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to look for |

## Return value

`true` when `object` has an entry, `false` otherwise.

## Complexity

Constant on average.

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
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    ranks[a] = 10;

    println("{} {}", ranks.contains(a), ranks.contains(b));
    ranks.erase(a);
    println("{}", ranks.contains(a));
}
```

Output:

```text
true false
false
```

## See also

- [count](count.md): the same question as a number
- [find](find.md): the entry of an object
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
