[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::count

```cpp
size_type count(const key_pointer& object) const noexcept;
```

Returns the number of entries of `object`: 1 when it has one, 0 when it has none, is gone or is null. The search is
that of [find](find.md); an object has at most one entry.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entries to count |

## Return value

The number of entries of `object`, 0 or 1.

## Complexity

Constant on average: the walk of the object's bucket, an entry or two.

## Exceptions

None.

## Notes

Wait-free once the object's bucket has its dummy node, as [find](find.md) is. Under concurrent insertions and
erasures of the same object the answer is of the moment of the search.

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
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    ranks.try_emplace(a, 10);
    ranks.try_emplace(a, 20);  // a has an entry: nothing added

    println("{} {}", ranks.count(a), ranks.count(b));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): the same question as a `bool`
- [find](find.md): the entry of an object
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)
