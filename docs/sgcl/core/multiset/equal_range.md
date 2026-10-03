[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::equal_range

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;                      // (1)
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;    // (2)
template<class K>
std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);            // (3)
template<class K>
std::pair<const_iterator, const_iterator> equal_range(const K& key) const                     // (4)
    noexcept(/* see below */);
```

Returns the run of the elements with the key `key`: they are adjacent in the order of iteration, from the first
of them to the element after the last.

- (3–4) The key is of any type the hash and the equality take, and no `Key` is built for the search. Take part
  only when `Hash` and `KeyEqual` both declare `is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../string/README.md) do.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements |

## Return value

The pair of iterators `[first, last)` of the elements with the key; both [end()](end.md) when the key is not
there.

## Complexity

Constant on average, the walk of one bucket, plus the number of elements with the key.

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
    multiset<string> s = {"pear", "apple", "pear", "pear"};
    auto [first, last] = s.equal_range("pear");
    string joined;
    for (auto it = first; it != last; ++it) {
        joined = joined + *it + ";";
    }
    println("{} {}", std::distance(first, last), joined);

    auto [none, end] = s.equal_range("fig");
    println("{} {}", none == s.end(), end == s.end());
}
```

Output:

```text
3 pear;pear;pear;
true true
```

## See also

- [count](count.md): the number of elements with a key
- [find](find.md): an iterator to the first element with a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
