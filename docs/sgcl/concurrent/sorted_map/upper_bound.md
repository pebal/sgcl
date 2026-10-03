[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](README.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::upper_bound

```cpp
iterator upper_bound(const Key& key) noexcept;                                // (1)
const_iterator upper_bound(const Key& key) const noexcept;                    // (2)
template<class K> iterator upper_bound(const K& key) noexcept;                // (3)
template<class K> const_iterator upper_bound(const K& key) const noexcept;    // (4)
```

Returns an iterator to the first element whose key is greater than `key`: the node the search of
[find](find.md) stops at on the bottom list, or the next element not erased after it when its key is equivalent
to `key`.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element whose key is greater than `key`, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the map, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing; a walk from the iterator is weakly consistent. Java's
`ConcurrentSkipListMap` asks the same with `higherEntry`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> events = {{5, "start"}, {10, "load"}, {15, "parse"}};

    println("{}", events.upper_bound(10)->first);
    println("{}", events.upper_bound(7)->first);
    println("{}", events.upper_bound(15) == events.end());
}
```

Output:

```text
15
10
true
```

## See also

- [lower_bound](lower_bound.md): the first element whose key is not less than a key
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](README.md)
