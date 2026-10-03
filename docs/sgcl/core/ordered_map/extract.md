[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::extract

```cpp
/*(1)*/ node_type extract(const_iterator pos) noexcept;
/*(2)*/ node_type extract(const key_type& key) noexcept;
/*(3)*/ template<class K> node_type extract(K&& key) noexcept(/* see below */);
```

Unlinks a node from the chain and from the order and hands it over in a
[node handle](../ordered_map-node_type.md), the element untouched: the handle owns it now, and destroys it if it
dies without having been inserted. Out of the map, the key may change (`key()` of the handle is writable);
[insert](insert.md) links the node again, here or in another `ordered_map` with the same `Key` and `T`, at the
end of the order.

1. The node at `pos`.
2. The node under `key`; an empty handle when the key is absent.
3. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to extract; not `end()` |
| `key` | the key of the element to extract |

## Return value

A node handle that holds the element, or an empty one.

## Complexity

- (1) Constant on average, linear in the size of the bucket of `pos`.
- (2–3) Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1–2) None.
- (3) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what those calls throw, before anything is unlinked.

## Notes

Only iterators to the extracted element are invalidated. The handle holds the node by a `tracked_ptr`, so it lives
on a stack or inside a managed object, as the map does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
    auto nh = m.extract(1);
    println("{} {} {}", m, nh.key(), nh.mapped());

    nh.key() = 7;  // the key may change outside a map
    m.insert(std::move(nh));  // at the end of the order; no string copied
    println("{} {}", m, nh.empty());

    println("{}", m.extract(42).empty());
}
```

Output:

```text
{2: "b", 3: "c"} 1 a
{2: "b", 3: "c", 7: "a"} true
true
```

## See also

- [node_type](../ordered_map-node_type.md): the node handle
- [insert](insert.md): links the node of a handle
- [merge](merge.md): relinks the nodes of another map
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
