[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::take

```cpp
optional<mapped_type> take(const key_type& key)                                          // (1)
    noexcept(std::is_nothrow_move_constructible_v<T>);
template<class K> optional<mapped_type> take(const K& key) noexcept(/* see below */);    // (2)
```

Moves the value under `key` out and erases the element, in one search; `nullopt` when the key is absent. What
Java's `remove(key)` and C#'s `Remove(key, out value)` hand back, where [extract](extract.md) gives the node
and [erase](erase.md) a count. The element leaves the order with its node.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both
   declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to take |

## Return value

The value that was under the key, or `nullopt`. `optional` is the alias of `std::optional`
([aliases](../aliases.md)).

## Complexity

Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1) What the move constructor of `T` throws; none when it is noexcept.
- (2) The same, and what the calls of `Hash` and `KeyEqual` with a `K` throw: none when they are noexcept or
  they are the function objects of `std`.

If an exception is thrown, the element is still in the map.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m = {{"a", 1}, {"b", 2}};

    optional<int> taken = m.take("a");
    optional<int> none = m.take("a");
    println("{} {} {}", *taken, none.has_value(), m);
}
```

Output:

```text
1 false {"b": 2}
```

## See also

- [erase](erase.md): erases elements
- [extract](extract.md): takes the node out, the element in it
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
