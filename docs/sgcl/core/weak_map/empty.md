[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map holds an entry. An entry whose object is gone and that no sweep has dropped yet is an entry:
`empty` answers for the table, as [size](size.md) counts it, not for the live objects.

## Parameters

None.

## Return value

`true` when the map holds no entry, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

The answer is exact for the live objects right after a [sweep](sweep.md). Whether the map has a live entry is what
[begin](begin.md) answers: `begin() == end()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

void rank_a_temporary(weak_map<Node, int>& ranks) {
    tracked_ptr node = make_tracked<Node>(1);
    ranks[node] = 10;
}

int main() {
    weak_map<Node, int> ranks;
    println("{}", ranks.empty());

    rank_a_temporary(ranks);
    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the node
    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", ranks.empty(), ranks.begin() == ranks.end());

    ranks.sweep();
    println("{}", ranks.empty());
}
```

Output:

```text
true
false true
true
```

## See also

- [size](size.md): the number of entries
- [sweep](sweep.md): erases the dead entries
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
