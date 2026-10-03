[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::extract

```cpp
node_type extract(const_iterator pos) noexcept;                            // (1)
node_type extract(const key_type& key) noexcept;                           // (2)
template<class K> node_type extract(K&& key) noexcept(/* see below */);    // (3)
```

Unlinks an element's node from the map and hands it over in a [node handle](../map-node_type.md), the element
untouched: the handle owns it from then on, and destroys it if the handle dies without having inserted it
anywhere.

1. The element at `pos`.
2. The element under `key`, if there is one.
3. As (2), with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

Out of the map, the key may be changed through the handle's [key()](../map-node_type/key.md), and the node
inserted again, here or into another map or [multimap](../multimap.md), by [insert](insert.md).

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to extract; not `end()` |
| `key` | the key of the element to extract |

## Return value

A node handle that holds the element, or an empty one when the key is absent.

## Complexity

Constant on average, linear in the size when every key falls into one bucket.

## Exceptions

- (1–2) None.
- (3) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what they throw.

## Notes

Nothing is copied or moved, either way: the node itself passes from the map to the handle and back. The iterator
to the extracted element is invalidated; a reference to the element stays valid while the handle or a map holds
the node.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, string> m = {{1, "a"}, {2, "b"}};
    auto nh = m.extract(1);
    println("{} {} {}", m.size(), nh.key(), nh.mapped());

    nh.key() = 7;  // the key may change outside a map
    m.insert(std::move(nh));
    println("{} {} {}", m.contains(1), m.at(7), nh.empty());

    println("{}", m.extract(42).empty());
}
```

Output:

```text
1 1 a
false a true
true
```

## See also

- [node_type](../map-node_type.md): the node handle
- [insert](insert.md): inserts a node handle's node
- [merge](merge.md): relinks the nodes of another map
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
