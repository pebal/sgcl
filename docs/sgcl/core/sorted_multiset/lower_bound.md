[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::lower_bound

```cpp
iterator lower_bound(const key_type& key) noexcept;                                            // (1)
const_iterator lower_bound(const key_type& key) const noexcept;                                // (2)
template<class K> iterator lower_bound(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator lower_bound(const K& key) const noexcept(/* see below */);    // (4)
```

Returns an iterator to the first element that is not less than `key`: the first of the elements with the key, or
the one after the place where they would be. From it, `++` walks the elements in order.

- (3–4) The key is of any type the comparison takes with a `Key`, and no `Key` is built for the search. Take part
  only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string/README.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element not less than `key`, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the multiset.

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
    sorted_multiset<int> sizes = {38, 40, 40, 42, 44};

    auto it = sizes.lower_bound(40);
    println("{} {}", *it, std::distance(sizes.begin(), it));
    println("{}", *sizes.lower_bound(41));
    println("{}", sizes.lower_bound(46) == sizes.end());
}
```

Output:

```text
40 1
42
true
```

## See also

- [upper_bound](upper_bound.md): the first element greater than a key
- [equal_range](equal_range.md): both bounds at once
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
