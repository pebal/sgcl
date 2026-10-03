[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::erase

```cpp
iterator erase(const_iterator pos) noexcept;                                                   // (1)
iterator erase(iterator pos) noexcept requires (!std::is_same_v<iterator, const_iterator>);    // (2)
iterator erase(const_iterator first, const_iterator last) noexcept;                            // (3)
size_type erase(const key_type& key) noexcept;                                                 // (4)
template<class K> size_type erase(K&& key) noexcept(/* see below */);                          // (5)
```

Erases elements. An erased element is destroyed at once and its node unlinked from the chain and from the order;
the collector reclaims the node later.

1. Erases the element at `pos`.
2. The same for an `iterator`.
3. Erases the elements of the range `[first, last)`, a range of the order.
4. Erases the element under `key`, if there is one.
5. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

The bucket count never shrinks on an erasure. An erasure during an iteration is written as with `std`:
`it = m.erase(it)`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase; not `end()` |
| `first`, `last` | the range of the elements to erase |
| `key` | the key of the element to erase |

## Return value

- (1–2) An iterator to the element after `pos` in the order, or `end()` when `pos` was the newest.
- (3) `last`.
- (4–5) The number of elements erased, 1 or 0.

## Complexity

- (1–2) Constant on average, linear in the size of the bucket of `pos`.
- (3) Linear in the number of elements erased, on average.
- (4–5) Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1–4) None.
- (5) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what those calls throw, before anything is erased.

## Notes

`pos` must address an element: `end()` of an `ordered_map` is its sentinel, not a null iterator, and erasing it
is undefined.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, string> m = {{4, "d"}, {1, "a"}, {3, "c"}, {2, "b"}, {5, "e"}};
    println("{}", m.erase(2));  // "b" is destroyed here

    for (auto it = m.begin(); it != m.end();) {
        it = it->first % 2 ? m.erase(it) : std::next(it);
    }
    println("{}", m);

    ordered_map<string, int> words = {{"x", 1}, {"y", 2}, {"z", 3}};
    auto it = words.erase(words.begin(), std::prev(words.end()));
    println("{} {}", it->first, words.erase("nothing"));
}
```

Output:

```text
1
{4: "d"}
z 0
```

## See also

- [take](take.md): erases the element under a key and hands its value back
- [extract](extract.md): takes the node out without destroying the element
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
