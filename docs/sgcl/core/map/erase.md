[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::erase

```cpp
iterator erase(const_iterator pos) noexcept;                                                   // (1)
iterator erase(iterator pos) noexcept requires (!std::is_same_v<iterator, const_iterator>);    // (2)
iterator erase(const_iterator first, const_iterator last) noexcept;                            // (3)
size_type erase(const key_type& key) noexcept;                                                 // (4)
template<class K> size_type erase(K&& key) noexcept(/* see below */);                          // (5)
```

Erases elements. Each is destroyed at once and its node unlinked; the collector reclaims the node later.

1. Erases the element at `pos`.
2. As (1), for an `iterator`.
3. Erases the elements of the range `[first, last)`.
4. Erases the element under `key`, if there is one.
5. As (4), with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

An erasure never shrinks the bucket array. A loop that erases as it goes takes the iterator (1) returns,
`it = m.erase(it)`, as with `std`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase; not `end()` |
| `first`, `last` | the range of the elements to erase |
| `key` | the key of the element to erase |

## Return value

- (1–3) An iterator to the element after the last one erased, or `end()`.
- (4–5) The number of elements erased, 1 or 0.

## Complexity

- (1–2) Constant on average: the walk of the element's bucket to the node before it.
- (3) Linear in the distance between `first` and `last`.
- (4–5) Constant on average, linear in the size when every key falls into one bucket.

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
#include <string_view>

using namespace sgcl;

int main() {
    map<int, string> m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
    size_t first = m.erase(2);  // "b" is destroyed here
    size_t again = m.erase(2);
    println("{} {}", first, again);

    for (auto it = m.begin(); it != m.end();) {
        it = it->first % 2 ? m.erase(it) : std::next(it);
    }
    println("{} {}", m.size(), m.at(4));

    m.erase(m.begin(), m.end());
    println("{} {}", m.empty(), m.bucket_count() > 0);

    map<string, int> ages = {{"ann", 31}};
    std::string_view name = "ann";
    println("{}", ages.erase(name));  // no string is built for the key
}
```

Output:

```text
1 0
1 d
true true
1
```

## See also

- [clear](clear.md): destroys every element
- [erase_if](erase_if.md): erases every element satisfying a predicate
- [take](take.md): moves the value under a key out and erases the element
- [extract](extract.md): unlinks an element into a node handle, without destroying it
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
