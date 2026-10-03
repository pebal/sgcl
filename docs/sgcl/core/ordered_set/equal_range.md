[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::equal_range

```cpp
/*(1)*/ std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;
/*(2)*/ std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;
/*(3)*/ template<class K>
        std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);
/*(4)*/ template<class K>
        std::pair<const_iterator, const_iterator> equal_range(const K& key) const
            noexcept(/* see below */);
```

Returns the range of the elements equal to `key`: the element and the one after it in the order when it is
there, an empty range otherwise. The elements being unique, the range holds one element or none.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the element to look for |

## Return value

A pair of iterators: the element and the next in the order (`end()` after the newest), or `end()` twice when the
element is not there.

## Complexity

Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what those calls throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {3, 1, 2};
    auto [first, last] = s.equal_range(1);
    println("{} {}", *first, *last);

    auto [none, end] = s.equal_range(9);
    println("{} {}", none == s.end(), end == s.end());
}
```

Output:

```text
1 2
true true
```

## See also

- [find](find.md): an iterator to an element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
