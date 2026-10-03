[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::erase

```cpp
size_type erase(const Key& key) noexcept;                    // (1)
template<class K> size_type erase(const K& key) noexcept;    // (2)
iterator erase(const_iterator pos) noexcept;                 // (3)
```

Erases an element.

1. Erases the element under `key`, if there is one.
2. The same with a key of another type. Takes part only when `Compare` declares `is_transparent` and `K` does not
   convert to `const_iterator`; no `Key` is built for the search.
3. Erases the element `pos` addresses, if it is still in the map. `pos` must address an element: `end()` may not
   be passed.

The node is marked at each of its levels, from the top down, by a marker node linked after it; the marker at the
bottom level is the erasure, and of several threads erasing one element, the one whose marker lands there has
erased it. The node is then unlinked: (1–2) with the predecessors of the search that found it, level by level,
and by a search again when one of them has changed; (3) by a search. The element itself lives on for as long as
some iterator holds its node, and dies with the node at a sweep.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to erase |
| `pos` | an iterator to the element to erase |

## Return value

- (1–2) The number of elements erased: 1, or 0 when the map holds no element under `key` or another thread erased
  it first.
- (3) An iterator to the element after `pos` in key order that is not erased, or [end()](end.md) when there is
  none; returned also when another thread had erased the element of `pos`.

## Complexity

Logarithmic in the size of the map, expected, plus a search again when a predecessor changed. One marker is
allocated per level of the node.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange that links the marker at the bottom level: of concurrent
erasures of one element, exactly one returns 1 (or erases it, for (3)). The thread that erases cannot know who is
reading the element, so it does not destroy it: the collector does, once nothing holds the node. An element that
must be released promptly is held by a `tracked_ptr` whose object does its own cleanup, or watched by an
[expiry_queue](../../core/expiry_queue.md).

`it = m.erase(it)` erases the elements a walk chooses, while other threads insert and erase, without a lock.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> tasks = {
        {1, "build"}, {2, "test"}, {3, "lint"}, {4, "deploy"}, {5, "notify"}};

    println("{} {}", tasks.erase(1), tasks.erase(9));

    auto held = tasks.find(2);
    for (auto it = tasks.begin(); it != tasks.end();) {
        it = it->first % 2 == 0 ? tasks.erase(it) : std::next(it);  // the even keys
    }
    println("{}", tasks);
    println("{}", held->second);  // erased, alive while `held` holds its node
}
```

Output:

```text
1 0
{3: "lint", 5: "notify"}
test
```

## See also

- [clear](clear.md): erases every element
- [insert](insert.md), [try_emplace](try_emplace.md): insert an element
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
