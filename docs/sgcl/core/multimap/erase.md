[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::erase

```cpp
/*(1)*/ iterator erase(const_iterator pos) noexcept;
/*(2)*/ iterator erase(iterator pos) noexcept requires (!std::is_same_v<iterator, const_iterator>);
/*(3)*/ iterator erase(const_iterator first, const_iterator last) noexcept;
/*(4)*/ size_type erase(const key_type& key) noexcept;
/*(5)*/ template<class K> size_type erase(K&& key) noexcept(/* see below */);
```

Erases elements. Each is destroyed at once and its node unlinked; the collector reclaims the node later.

1. Erases the element at `pos`.
2. As (1), for an `iterator`.
3. Erases the elements of the range `[first, last)`.
4. Erases every element under `key`, which may be the key of one of them (`m.erase(it->first)`): the run is found
   before anything is erased.
5. As (4), with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

An erasure never shrinks the bucket array. A loop that erases as it goes takes the iterator (1) returns,
`it = m.erase(it)`, as with `std`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase; not `end()` |
| `first`, `last` | the range of the elements to erase |
| `key` | the key of the elements to erase |

## Return value

- (1–3) An iterator to the element after the last one erased, or `end()`.
- (4–5) The number of elements erased.

## Complexity

- (1–2) Constant on average: the walk of the element's bucket to the node before it.
- (3) Linear in the distance between `first` and `last`.
- (4–5) Constant on average, plus the number of elements erased.

## Exceptions

- (1–4) None.
- (5) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what they throw.

## Notes

Only the iterators to the erased elements are invalidated. An iterator to an erased element keeps the node's
memory mapped but not the element.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, string> m = {{1, "a"}, {1, "b"}, {2, "c"}, {3, "d"}};
    size_t erased = m.erase(1);  // "a" and "b" are destroyed here
    println("{} {}", erased, m.size());

    m.erase(m.find(2));
    println("{} {}", m.contains(2), m.size());

    m.erase(m.begin(), m.end());
    println("{}", m.empty());
}
```

Output:

```text
2 2
false 1
true
```

## See also

- [clear](clear.md): destroys every element
- [erase_if](erase_if.md): erases every element satisfying a predicate
- [extract](extract.md): unlinks an element into a node handle, without destroying it
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
