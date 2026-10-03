[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::erase

```cpp
iterator erase(iterator pos) noexcept requires (!std::is_same_v<iterator, const_iterator>);    // (1)
iterator erase(const_iterator pos) noexcept;                                                   // (2)
iterator erase(const_iterator first, const_iterator last) noexcept;                            // (3)
size_type erase(const key_type& key) noexcept;                                                 // (4)
template<class K> size_type erase(K&& key) noexcept(/* see below */);                          // (5)
```

Erases elements. Each is destroyed at once and its node unlinked; the node's memory is reclaimed by the collector
later.

1. Erases the element `pos` addresses, which must be an element of this multimap, not `end()`. The clause keeps
   the overload apart from (2) where the two iterators are one type (the sets'); in a multimap they differ.
2. The same with a `const_iterator`.
3. Erases the elements of the range `[first, last)`. Erasing `[begin(), end())` is a [clear](clear.md).
4. Erases every element under `key`.
5. As (4), with a key of another type, compared without building a `key_type`. Takes part only when `Compare`
   declares `is_transparent`, as `std::less` of a [string](../string.md) does, and `K` converts to neither
   `iterator` nor `const_iterator`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase |
| `first`, `last` | the range of the elements to erase |
| `key` | the key of the elements to erase |

## Return value

- (1–3) An iterator to the element after the last one erased, or `end()`.
- (4–5) The number of elements erased.

## Complexity

- (1–2) Amortized constant.
- (3) Linear in the number of elements erased, plus logarithmic in the size of the multimap.
- (4–5) Logarithmic in the size of the multimap, plus linear in the number of elements erased.

## Exceptions

- (1–4) None.
- (5) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a
  function object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    sorted_multimap<int, string> m = {{1, "a"}, {1, "b"}, {2, "c"}, {3, "d"}, {3, "e"}};
    println("{}", m.erase(1));  // "a" and "b" are destroyed here

    for (auto it = m.begin(); it != m.end();) {
        it = it->second == "d" ? m.erase(it) : std::next(it);
    }
    println("{}", m);
}
```

Output:

```text
2
{2: "c", 3: "e"}
```

## See also

- [extract](extract.md): takes a node out without destroying the element
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
