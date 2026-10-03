[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::erase

```cpp
size_type erase(const Key& key) noexcept;                    // (1)
template<class K> size_type erase(const K& key) noexcept;    // (2)
iterator erase(const_iterator pos) noexcept;                 // (3)
```

Erases a key.

1. Erases the key equivalent to `key`, if the set holds one.
2. The same with a key of another type. Takes part only when `Compare` declares `is_transparent` and `K` does not
   convert to `const_iterator`; no `Key` is built for the search.
3. Erases the key `pos` addresses, if it is still in the set. `pos` must address a key: `end()` may not be
   passed.

The node is marked at each of its levels, from the top down, by a marker node linked after it; the marker at the
bottom level is the erasure, and of several threads erasing one key, the one whose marker lands there has erased
it. The node is then unlinked: (1–2) with the predecessors of the search that found it, level by level, and by a
search again when one of them has changed; (3) by a search. The key itself lives on for as long as some iterator
holds its node, and dies with the node at a sweep.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to erase |
| `pos` | an iterator to the key to erase |

## Return value

- (1–2) The number of keys erased: 1, or 0 when the set holds no key equivalent to `key` or another thread erased
  it first.
- (3) An iterator to the key after `pos` that is not erased, or [end()](end.md) when there is none; returned also
  when another thread had erased the key of `pos`.

## Complexity

Logarithmic in the size of the set, expected, plus a search again when a predecessor changed. One marker is
allocated per level of the node.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange that links the marker at the bottom level: of concurrent
erasures of one key, exactly one returns 1 (or erases it, for (3)). The thread that erases cannot know who is
reading the key, so it does not destroy it: the collector does, once nothing holds the node.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    concurrent::sorted_set<int> numbers = {1, 2, 3, 4, 5, 6};

    println("{} {}", numbers.erase(1), numbers.erase(9));

    for (auto it = numbers.begin(); it != numbers.end();) {
        it = *it % 2 == 0 ? numbers.erase(it) : std::next(it);  // the even keys
    }
    println("{}", numbers);
}
```

Output:

```text
1 0
{3, 5}
```

## See also

- [clear](clear.md): erases every key
- [insert](insert.md): inserts a key
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
