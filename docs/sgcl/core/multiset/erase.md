[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::erase

```cpp
/*(1)*/ iterator erase(const_iterator pos) noexcept;
/*(2)*/ iterator erase(const_iterator first, const_iterator last) noexcept;
/*(3)*/ size_type erase(const key_type& key) noexcept;
/*(4)*/ template<class K> size_type erase(K&& key) noexcept(/* see below */);
```

Erases elements. Each is destroyed at once and its node unlinked; the collector reclaims the node later.

1. Erases the element at `pos`, one of a run of equal elements alone.
2. Erases the elements of the range `[first, last)`.
3. Erases every element with the key `key`, the whole run, in one walk; `key` may be one of them
   (`s.erase(*it)`): the run is found before anything is erased.
4. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

`iterator` and `const_iterator` being one type, (1) takes either.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase; `end()` erases nothing and is returned |
| `first`, `last` | the range of the elements to erase |
| `key` | the key of the elements to erase |

## Return value

- (1) An iterator to the element after the erased one.
- (2) `last`.
- (3–4) The number of elements erased.

## Complexity

- (1) Constant on average, the walk of the element's bucket to its predecessor.
- (2) Linear in the number of elements erased on average.
- (3–4) Constant on average, the walk of one bucket, plus the number of elements erased.

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
    multiset s = {1, 1, 2, 3, 3};
    println("{} {}", s.erase(1), s.erase(1));

    s.erase(s.find(3));  // one of the two
    println("{} {}", s.size(), s.count(3));

    multiset<string> words = {"pear", "pear", "plum"};
    println("{}", words.erase("pear"));  // a literal: no string made for the search
    words.erase(words.begin(), words.end());
    println("{}", words.empty());
}
```

Output:

```text
2 0
2 1
2
true
```

## See also

- [clear](clear.md): erases every element
- [extract](extract.md): takes an element out without destroying it
- [erase_if](erase_if.md): erases the elements satisfying a predicate
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
