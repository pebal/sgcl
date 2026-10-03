[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::lower_bound

```cpp
/*(1)*/ iterator lower_bound(const Key& key) noexcept;
/*(2)*/ const_iterator lower_bound(const Key& key) const noexcept;
/*(3)*/ template<class K> iterator lower_bound(const K& key) noexcept;
/*(4)*/ template<class K> const_iterator lower_bound(const K& key) const noexcept;
```

Returns an iterator to the first key of the set that is not less than `key`: the node the search of
[find](find.md) stops at on the bottom list. From it, `++` walks the keys in order.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the keys of the set with |

## Return value

An iterator to the first key not less than `key`, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the set, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing. A walk from the iterator is weakly consistent, as every walk of the set
is: it sees the keys in order, skips the ones erased since it passed them and may or may not see the ones
inserted meanwhile. Java's `ConcurrentSkipListSet` asks the same with `ceiling`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<int> stamps = {5, 10, 15, 20, 25};

    for (auto it = stamps.lower_bound(10); it != stamps.end() && *it < 20; ++it) {
        println("{}", *it);  // the stamps in [10, 20)
    }
    println("{}", *stamps.lower_bound(12));
    println("{}", stamps.lower_bound(30) == stamps.end());
}
```

Output:

```text
10
15
15
true
```

## See also

- [upper_bound](upper_bound.md): the first key greater than a key
- [find](find.md): finds a key
- [sgcl::concurrent::sorted_set\<Key, Compare\>](../sorted_set.md)
