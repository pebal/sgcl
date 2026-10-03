[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::lower_bound

```cpp
/*(1)*/ iterator lower_bound(const Key& key) noexcept;
/*(2)*/ const_iterator lower_bound(const Key& key) const noexcept;
/*(3)*/ template<class K> iterator lower_bound(const K& key) noexcept;
/*(4)*/ template<class K> const_iterator lower_bound(const K& key) const noexcept;
```

Returns an iterator to the first element whose key is not less than `key`: the node the search of
[find](find.md) stops at on the bottom list. From it, `++` walks the elements in key order.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element whose key is not less than `key`, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the map, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing. A walk from the iterator is weakly consistent, as every walk of the map
is: it sees the elements in key order, skips the ones erased since it passed them and may or may not see the ones
inserted meanwhile. Java's `ConcurrentSkipListMap` asks the same with `ceilingEntry`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> events = {
        {5, "start"}, {10, "load"}, {15, "parse"}, {20, "run"}, {25, "stop"}};

    for (auto it = events.lower_bound(10); it != events.end() && it->first < 20; ++it) {
        println("{} {}", it->first, it->second);  // the keys in [10, 20)
    }
    println("{}", events.lower_bound(12)->first);
    println("{}", events.lower_bound(30) == events.end());
}
```

Output:

```text
10 load
15 parse
15
true
```

## See also

- [upper_bound](upper_bound.md): the first element whose key is greater than a key
- [find](find.md): the element under a key
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
