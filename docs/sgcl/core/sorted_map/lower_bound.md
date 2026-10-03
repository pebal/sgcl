[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::lower_bound

```cpp
/*(1)*/ iterator lower_bound(const key_type& key) noexcept;
/*(2)*/ const_iterator lower_bound(const key_type& key) const noexcept;
/*(3)*/ template<class K> iterator lower_bound(const K& key) noexcept(/* see below */);
/*(4)*/ template<class K> const_iterator lower_bound(const K& key) const noexcept(/* see below */);
```

Returns an iterator to the first element whose key is not less than `key`, as in `std::map`. From it, `++` walks
the elements in key order.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element whose key is not less than `key`, or [end()](end.md) when there is none.

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
    sorted_map<int, string> events = {{5, "start"}, {10, "load"}, {15, "parse"}, {20, "run"}};
    for (auto it = events.lower_bound(8), to = events.upper_bound(15); it != to; ++it) {
        println("{} {}", it->first, it->second);  // the keys in [8, 15]
    }
    println("{} {}", events.lower_bound(10)->first, events.lower_bound(30) == events.end());
}
```

Output:

```text
10 load
15 parse
10 true
```

## See also

- [upper_bound](upper_bound.md): the first element whose key is greater than a key
- [equal_range](equal_range.md): both, from one search
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
