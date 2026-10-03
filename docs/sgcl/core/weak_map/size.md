[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of entries, the dead ones not yet swept included: the table's count, which a sweep brings down
to the live objects.

## Parameters

None.

## Return value

The number of entries, live and dead.

## Complexity

Constant.

## Exceptions

None.

## Notes

The number is exact for the live objects right after a [sweep](sweep.md); between the sweeps it counts too the
entries whose objects have gone since. The number of live objects is what a walk from [begin](begin.md) counts.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

void rank_a_temporary(weak_map<Node, int>& ranks) {
    tracked_ptr node = make_tracked<Node>(2);
    ranks[node] = 20;
}

int main() {
    weak_map<Node, int> ranks;
    tracked_ptr kept = make_tracked<Node>(1);
    ranks[kept] = 10;
    rank_a_temporary(ranks);
    println("{}", ranks.size());

    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the node
    collector::force_collect(true);  // optional, for the demonstration
    println("{}", ranks.size());

    ranks.sweep();
    println("{}", ranks.size());
}
```

Output:

```text
2
2
1
```

## See also

- [empty](empty.md): checks whether the map holds an entry
- [sweep](sweep.md): erases the dead entries
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
