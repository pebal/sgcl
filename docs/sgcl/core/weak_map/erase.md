[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::erase

```cpp
size_type erase(const key_pointer& object) noexcept;    // (1)
iterator erase(iterator pos) noexcept;                  // (2)
```

Erases an entry and destroys its value at once.

1. Erases the entry of `object`, if it has one. A null pointer has none.
2. Erases the entry `pos` stands on, and returns the next live entry up to the iterator's own bound, `end()`. `pos`
   must stand on an entry, not be [end](end.md).

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entry to erase |
| `pos` | an iterator to the entry to erase |

## Return value

- (1) The number of entries erased, 0 or 1.
- (2) An iterator to the next live entry after `pos`, or `end()`.

## Complexity

- (1) Constant on average.
- (2) Constant on average, plus the dead entries before the next live one, which the iterator passes.

## Exceptions

None.

## Notes

An entry whose object is gone needs no erasure: it is never found, and a [sweep](sweep.md) drops it.

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
    vector<tracked_ptr<Node>> nodes;
    for (int i : range(4)) {
        nodes.push_back(make_tracked<Node>(i));
        ranks[nodes.back()] = i * 10;
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
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
