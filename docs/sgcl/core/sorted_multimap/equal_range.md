[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::equal_range

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;                      // (1)
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;    // (2)
template<class K>
std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);            // (3)
template<class K>
std::pair<const_iterator, const_iterator> equal_range(const K& key) const                     // (4)
    noexcept(/* see below */);
```

Returns the run of the elements under `key`, in the order they were inserted: [lower_bound](lower_bound.md) its
start, [upper_bound](upper_bound.md) its end, from one search. When the multimap holds no element under `key`,
the range is empty: two iterators to the first element whose key is greater.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

A pair of iterators: the first element whose key is not less than `key`, and the first whose key is greater.

## Complexity

Logarithmic in the size of the multimap.

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
    sorted_multimap<int, char> m = {{1, 'a'}, {2, 'b'}, {2, 'c'}, {3, 'd'}};
    auto [from, to] = m.equal_range(2);
    for (auto it = from; it != to; ++it) {
        print("{} ", it->second);
    }
    println("| {}", to->second);
}
```

Output:

```text
b c | d
```

## See also

- [count](count.md): the number of elements under a key
- [lower_bound](lower_bound.md), [upper_bound](upper_bound.md): the ends of the run
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
