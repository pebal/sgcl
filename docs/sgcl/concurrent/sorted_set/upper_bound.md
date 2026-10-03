[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::upper_bound

```cpp
/*(1)*/ iterator upper_bound(const Key& key) noexcept;
/*(2)*/ const_iterator upper_bound(const Key& key) const noexcept;
/*(3)*/ template<class K> iterator upper_bound(const K& key) noexcept;
/*(4)*/ template<class K> const_iterator upper_bound(const K& key) const noexcept;
```

Returns an iterator to the first key of the set that is greater than `key`: the node the search of
[find](find.md) stops at on the bottom list, or the next key not erased after it when it is equivalent to `key`.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the keys of the set with |

## Return value

An iterator to the first key greater than `key`, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the set, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing; a walk from the iterator is weakly consistent. Java's
`ConcurrentSkipListSet` asks the same with `higher`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<int> stamps = {5, 10, 15};

    println("{}", *stamps.upper_bound(10));
    println("{}", *stamps.upper_bound(7));
    println("{}", stamps.upper_bound(15) == stamps.end());
}
```

Output:

```text
15
10
true
```

## See also

- [lower_bound](lower_bound.md): the first key not less than a key
- [sgcl::concurrent::sorted_set\<Key, Compare\>](../sorted_set.md)
