[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::erase

```cpp
iterator erase(const_iterator pos) noexcept;                             // (1)
iterator erase(const_iterator first, const_iterator last) noexcept;      // (2)
size_type erase(const key_type& key) noexcept;                           // (3)
template<class K> size_type erase(K&& key) noexcept(/* see below */);    // (4)
```

Erases elements. Each is destroyed at once and its node unlinked; the collector reclaims the node later.

1. Erases the element at `pos`.
2. Erases the elements of the range `[first, last)`.
3. Erases the element with the key `key`, if there is one.
4. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

`iterator` and `const_iterator` being one type, (1) takes either.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase; `end()` erases nothing and is returned |
| `first`, `last` | the range of the elements to erase |
| `key` | the key of the element to erase |

## Return value

- (1) An iterator to the element after the erased one.
- (2) `last`.
- (3–4) The number of elements erased, 0 or 1.

## Complexity

- (1) Constant on average, the walk of the element's bucket to its predecessor.
- (2) Linear in the number of elements erased on average.
- (3–4) Constant on average, the walk of one bucket.

## Exceptions

- (1–3) None.
- (4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are std's function objects;
  otherwise what they throw.

## Notes

Erasing during an iteration is `it = s.erase(it)`. The bucket count never shrinks on an erase: a
[rehash](rehash.md) gives the buckets back.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set s = {1, 2, 3, 4, 5};
    println("{} {}", s.erase(2), s.erase(2));

    for (auto it = s.begin(); it != s.end();) {
        it = *it % 2 ? s.erase(it) : std::next(it);  // 1, 3 and 5 go
    }
    println("{} {}", s.size(), s.contains(4));

    set<string> words = {"apple", "pear", "plum"};
    println("{}", words.erase("pear"));  // a literal: no string made for the search
    words.erase(words.begin(), words.end());
    println("{} {}", words.empty(), words.bucket_count());
}
```

Output:

```text
1 0
1 true
1
true 4
```

## See also

- [clear](clear.md): erases every element
- [extract](extract.md): takes an element out without destroying it
- [erase_if](erase_if.md): erases the elements satisfying a predicate
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
