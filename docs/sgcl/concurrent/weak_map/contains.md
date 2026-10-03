[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::contains

```cpp
bool contains(const key_pointer& object) const noexcept;
```

Checks whether `object` has an entry: the search of [find](find.md), without an iterator. A null pointer has no
entry, and an object that is gone has none.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to look for |

## Return value

`true` when `object` has an entry, `false` otherwise.

## Complexity

Constant on average: the walk of the object's bucket, an entry or two.

## Exceptions

None.

## Notes

Wait-free once the object's bucket has its dummy node, as [find](find.md) is. Under concurrent insertions and
erasures of the same object the answer is of the moment of the search: a thread that wants the value takes it with
`find`, or adds it with [try_emplace](try_emplace.md), whose answer is the entry itself.

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
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)
