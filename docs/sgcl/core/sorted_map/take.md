[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::take

```cpp
optional<mapped_type> take(const key_type& key)                                          // (1)
    noexcept(std::is_nothrow_move_constructible_v<T>);
template<class K> optional<mapped_type> take(const K& key) noexcept(/* see below */);    // (2)
```

Moves the value under `key` out of the map and erases the element; returns `nullopt` when the map does not hold
the key.

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

The value that was under `key`, or `nullopt`.

## Complexity

Logarithmic in the size of the map.

## Exceptions

- What the move constructor of `T` throws; none when it is noexcept.
- (2) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a function object
  of `std`.

If an exception is thrown, the element stays in the map.

## Notes

`take` is what Java's `remove(key)` and C#'s `Remove(key, out value)` hand back, and what `std` has no word for:
[extract](extract.md) gives the node, [erase](erase.md) a count.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> m = {{"a", 1}, {"b", 2}};
    optional<int> taken = m.take("a");
    println("{} {}", taken, m);

    optional<int> none = m.take("a");
    println("{}", none);
}
```

Output:

```text
1 {"b": 2}
nullopt
```

## See also

- [erase](erase.md): erases elements
- [extract](extract.md): takes a node out of the map, into a node handle
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
