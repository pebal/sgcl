[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::take

```cpp
/*(1)*/ optional<mapped_type> take(const key_type& key)
            noexcept(std::is_nothrow_move_constructible_v<T>);
/*(2)*/ template<class K> optional<mapped_type> take(const K& key) noexcept(/* see below */);
```

Moves the value under `key` out of the map and erases the element, in one walk of the key's bucket; nothing
happens when the key is absent.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to take |

## Return value

The value that was under the key, or `nullopt` when the key is absent.

## Complexity

Constant on average, linear in the size when every key falls into one bucket.

## Exceptions

- (1) What the move constructor of `T` throws; none when it is noexcept.
- (2) The same, and what the calls of `Hash` and `KeyEqual` with a `K` throw; none from them when they are
  noexcept or the function objects of `std`.

If the move throws, the element stays in the map.

## Notes

`take` has no counterpart in `std::unordered_map`: it is what Java's `remove(key)` and C#'s
`Remove(key, out value)` hand back, where [extract](extract.md) gives the whole node and [erase](erase.md) a
count.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, vector<int>> m = {{"a", {1, 2, 3}}};
    optional<vector<int>> taken = m.take("a");
    println("{} {}", *taken, m.empty());

    optional<vector<int>> none = m.take("a");
    println("{}", none.has_value());
}
```

Output:

```text
[1, 2, 3] true
false
```

## See also

- [erase](erase.md): erases elements
- [extract](extract.md): unlinks an element into a node handle
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
