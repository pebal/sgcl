[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::erase

```cpp
/*(1)*/ map erase(const Key& key) const noexcept(std::is_nothrow_copy_constructible_v<value_type>);
/*(2)*/ template<class K> map erase(const K& key) const noexcept(/* see below */);
```

Returns the map without the element under `key`. This map is unchanged. The path to the element is copied, a node
emptied is dropped, and a subtrie left with one element is folded into its parent, so a map erased down to
nothing holds no node. When the key is absent, the result is this same map, sharing everything.

1. Erases the element under `key`.
2. Erases the element under a key of another type without building a `Key`. Takes part only when
   `Hash::is_transparent` and `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to erase |

## Return value

The new map, one element fewer; this map when the key was absent.

## Complexity

Logarithmic in `size()`, base 32: the nodes on the path copied, up to 32 elements each; nothing copied when the
key is absent.

## Exceptions

- (1) What the copy of the elements throws; none when it is noexcept.
- (2) The same, and what `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as
  those of the function objects of `std` are taken to be.

This map is never changed, so an exception leaves it as it was; no new map is made.

## Notes

Nothing is destroyed: this map still holds the element, and the collector destroys it with its node once no
version reaches the node. 670 ns per erase over a hundred thousand `int`s.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> ports = {{"http", 80}, {"https", 443}};
    auto fewer = ports.erase("http");  // a literal: no string made
    println("{} {} {}", ports.size(), fewer.size(), fewer.contains("http"));
    println("{}", ports.erase("ftp") == ports);
}
```

Output:

```text
2 1 false
true
```

## See also

- [insert](insert.md), [set](set.md): the map with an element more
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
