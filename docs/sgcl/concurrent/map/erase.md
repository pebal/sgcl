[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::erase

```cpp
/*(1)*/ size_type erase(const Key& key) noexcept;
/*(2)*/ template<class K> size_type erase(const K& key) noexcept;
/*(3)*/ iterator erase(const_iterator pos) noexcept;
```

Erases an element. The node is marked first, a marker node linked after it with a compare-exchange, and then
unlinked: with a compare-exchange on its predecessor, or, when the predecessor has moved on, by the next search
that passes it, as every search unlinks the marked nodes it meets.

1. Erases the element under `key`, if there is one.
2. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` is not convertible to `const_iterator`.
3. Erases the element `pos` addresses, if it is still there: another thread may have erased it, and a new element
   under the same key is another node, which (3) leaves.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to erase |
| `pos` | an iterator to the element to erase; not `end()` |

## Return value

- (1–2) The number of elements erased, 1 or 0.
- (3) An iterator to the element after `pos` in the list, or [end()](end.md).

## Complexity

Constant on average: the search of the key's bucket, a compare-exchange to mark the node and one to unlink it.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange that marks the node: of several threads erasing one element,
exactly one marks it and erases it, and the others get 0. The element is not destroyed by the erase: another thread
may be reading it, through an iterator or in the middle of a search, and the collector destroys it with its node
once nothing holds the node. An iterator to the element stays valid and reads it as it was.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<string, int> stock = {{"apple", 3}, {"pear", 0}, {"plum", 5}, {"fig", 0}};

    auto held = stock.find("apple");
    println("{} {}", stock.erase("apple"), stock.erase("apple"));
    println("{} {}", held->first, held->second);  // the element outlives its erasure

    for (auto it = stock.begin(); it != stock.end();) {
        if (it->second == 0) {
            it = stock.erase(it);
        } else {
            ++it;
        }
    }
    println("{} {}", stock.size(), stock.contains("plum"));
}
```

Output:

```text
1 0
apple 3
1 true
```

## See also

- [clear](clear.md): erases every element
- [insert](insert.md), [try_emplace](try_emplace.md): insert elements
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
