[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::upper_bound

```cpp
iterator upper_bound(const key_type& key) noexcept;                                            // (1)
const_iterator upper_bound(const key_type& key) const noexcept;                                // (2)
template<class K> iterator upper_bound(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator upper_bound(const K& key) const noexcept(/* see below */);    // (4)
```

Returns an iterator to the first element that is greater than `key`. With [lower_bound](lower_bound.md) it bounds
the elements of a closed range of keys: `[lower_bound(a), upper_bound(b))` holds every key from `a` to `b`.

- (3–4) The key is of any type the comparison takes with a `Key`, and no `Key` is built for the search. Take part
  only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element greater than `key`, or [end()](end.md) when there is none.

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
#include <iterator>

using namespace sgcl;

int main() {
    sorted_set<int> years = {1969, 1984, 1991, 2001, 2012};

    auto from = years.lower_bound(1984);
    auto to = years.upper_bound(2001);
    println("{}", std::distance(from, to));  // 1984, 1991 and 2001

    println("{}", *years.upper_bound(1991));
    println("{}", years.upper_bound(2012) == years.end());
}
```

Output:

```text
3
2001
true
```

## See also

- [lower_bound](lower_bound.md): the first element not less than a key
- [equal_range](equal_range.md): both bounds at once
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
