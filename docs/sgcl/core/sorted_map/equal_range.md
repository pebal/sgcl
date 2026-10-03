[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::equal_range

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;                      // (1)
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;    // (2)
template<class K>
std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);            // (3)
template<class K>
std::pair<const_iterator, const_iterator> equal_range(const K& key) const                     // (4)
    noexcept(/* see below */);
```

Returns the range of the elements under `key`: [lower_bound](lower_bound.md) and
[upper_bound](upper_bound.md) together, from one search. The keys being unique, the range holds one element or
none; an empty range is two iterators to the first element whose key is greater.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

A pair of iterators: the first element whose key is not less than `key`, and the first whose key is greater.

## Complexity

Logarithmic in the size of the map.

## Exceptions

- (1–2) None.
- (3–4) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a function
  object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<int, char> m = {{10, 'a'}, {20, 'b'}, {30, 'c'}};
    auto [first, last] = m.equal_range(20);
    println("{} {}", first->second, last->second);

    auto [from, to] = m.equal_range(25);  // empty: both at 30
    println("{} {}", from == to, from->first);
}
```

Output:

```text
b c
true 30
```

## See also

- [lower_bound](lower_bound.md): the first element whose key is not less than a key
- [upper_bound](upper_bound.md): the first element whose key is greater than a key
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
