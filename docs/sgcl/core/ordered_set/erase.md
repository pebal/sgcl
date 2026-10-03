[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::erase

```cpp
iterator erase(const_iterator pos) noexcept;                             // (1)
iterator erase(const_iterator first, const_iterator last) noexcept;      // (2)
size_type erase(const key_type& key) noexcept;                           // (3)
template<class K> size_type erase(K&& key) noexcept(/* see below */);    // (4)
```

Erases elements. An erased element is destroyed at once and its node unlinked from the chain and from the order;
the collector reclaims the node later.

1. Erases the element at `pos`. `iterator` and `const_iterator` being one type, there is no other overload for
   an `iterator`.
2. Erases the elements of the range `[first, last)`, a range of the order.
3. Erases the element equal to `key`, if there is one.
4. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

The bucket count never shrinks on an erasure. An erasure during an iteration is written as with `std`:
`it = s.erase(it)`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase; not `end()` |
| `first`, `last` | the range of the elements to erase |
| `key` | the element to erase |

## Return value

- (1) An iterator to the element after `pos` in the order, or `end()` when `pos` was the newest.
- (2) `last`.
- (3–4) The number of elements erased, 1 or 0.

## Complexity

- (1) Constant on average, linear in the size of the bucket of `pos`.
- (2) Linear in the number of elements erased, on average.
- (3–4) Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1–3) None.
- (4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what those calls throw, before anything is erased.

## Notes

`pos` must address an element: `end()` of an `ordered_set` is its sentinel, not a null iterator, and erasing it
is undefined.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {4, 1, 3, 2, 5};
    println("{}", s.erase(2));

    for (auto it = s.begin(); it != s.end();) {
        it = *it % 2 ? s.erase(it) : std::next(it);
    }
    println("{}", s);

    ordered_set<string> words = {"x", "y", "z"};
    auto it = words.erase(words.begin(), std::prev(words.end()));
    println("{} {}", *it, words.erase("nothing"));
}
```

Output:

```text
1
{4}
z 0
```

## See also

- [extract](extract.md): takes the node out without destroying the element
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
