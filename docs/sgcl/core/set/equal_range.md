[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::equal_range

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;                      // (1)
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;    // (2)
template<class K>
std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);            // (3)
template<class K>
std::pair<const_iterator, const_iterator> equal_range(const K& key) const                     // (4)
    noexcept(/* see below */);
```

Returns the range of the elements with the key `key`: the element and the one after it in the iteration, or an
empty range. In a set the range holds one element at most; the function is there for the code written for a
[multiset](../multiset.md) as well.

- (3–4) The key is of any type the hash and the equality take, and no `Key` is built for the search. Take part
  only when `Hash` and `KeyEqual` both declare `is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../string.md) do.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements |

## Return value

The pair of iterators `[first, last)` of the elements with the key; both [end()](end.md) when the key is not
there.

## Complexity

Constant on average, the walk of one bucket.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are std's function objects;
  otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<string> s = {"apple", "pear"};
    auto [first, last] = s.equal_range("pear");
    println("{} {}", *first, std::distance(first, last));

    auto [none, end] = s.equal_range("fig");
    println("{} {}", none == s.end(), end == s.end());
}
```

Output:

```text
pear 1
true true
```

## See also

- [find](find.md): an iterator to the element with a key
- [count](count.md): the number of elements with a key
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
