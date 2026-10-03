[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::equal_range

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;                      // (1)
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;    // (2)
template<class K>
std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);            // (3)
template<class K>
std::pair<const_iterator, const_iterator> equal_range(const K& key) const                     // (4)
    noexcept(/* see below */);
```

Returns the range of the elements equivalent to `key`: [lower_bound](lower_bound.md) and
[upper_bound](upper_bound.md) of the key, found in one search. In a set the range holds one element or none.

- (3–4) The key is of any type the comparison takes with a `Key`, and no `Key` is built for the search. Take part
  only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements |

## Return value

A pair of iterators: the first element not less than `key`, and the first element greater than it. Both are the
same iterator when there is no element with the key. `std::pair` is `pair` ([aliases](../aliases.md)).

## Complexity

Logarithmic in the size of the set.

## Exceptions

- (1–2) None.
- (3–4) What the calls of `Compare` with a `K` throw; none when they are noexcept, or when `Compare` is a function
  object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_set<int> numbers = {10, 20, 30};

    auto [first, last] = numbers.equal_range(20);
    println("{} {}", *first, *last);

    auto [from, to] = numbers.equal_range(25);
    println("{} {}", from == to, *from);
}
```

Output:

```text
20 30
true 30
```

## See also

- [lower_bound](lower_bound.md), [upper_bound](upper_bound.md): the two ends of the range
- [find](find.md): the element with a key
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
