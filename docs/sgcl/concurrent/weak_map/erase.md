[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](../weak_map.md)

# sgcl::concurrent::weak_map\<Key, T\>::erase

```cpp
size_type erase(const key_pointer& object) noexcept;    // (1)
iterator erase(iterator pos) noexcept;                  // (2)
```

Erases an entry.

1. Erases the entry of `object`, if it has one. A null pointer has none, and an object that is gone has none.
2. Erases the entry `pos` stands on, if it is still there: another thread may have erased it since. `pos` must stand
   on an entry, not be [end](end.md).

The node is marked, with a compare-exchange that links a marker node after it, and then unlinked from the list by a
search: by the hash the entry was placed with, which it carries, so an entry is erased from where it was put. The
value is not destroyed: the collector destroys it with the node, once nothing holds the node.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entry to erase |
| `pos` | an iterator to the entry to erase |

## Return value

- (1) The number of entries erased, 0 or 1.
- (2) An iterator to the next live entry after `pos`, or [end](end.md).

## Complexity

- (1) Constant on average: the walk of the object's bucket, an entry or two.
- (2) Constant on average, plus the walk to the next live entry.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange that marks the node: of several threads erasing the same entry,
exactly one marks it, and (1) returns 1 to that one alone. A reader standing on the entry keeps reading it, its
value included: an iterator holds its node.

An entry whose object is gone needs no erasure: it is never found, and a [sweep](sweep.md) drops it.

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
    vector<tracked_ptr<Node>> nodes;
    for (int i : range(4)) {
        nodes.push_back(make_tracked<Node>(i));
        ranks.try_emplace(nodes.back(), i * 10);
    }

    size_t first = ranks.erase(nodes[0]);
    size_t again = ranks.erase(nodes[0]);
    println("{} {} {}", first, again, ranks.size());

    for (auto it = ranks.begin(); it != ranks.end();) {
        if (it->value >= 20) {
            it = ranks.erase(it);
        } else {
            ++it;
        }
    }
    println("{} {}", ranks.size(), ranks.contains(nodes[1]));
}
```

Output:

```text
1 0 3
1 true
```

## See also

- [sweep](sweep.md): erases the entries whose objects are gone
- [clear](clear.md): erases every entry
- [sgcl::concurrent::weak_map\<Key, T\>](../weak_map.md)
