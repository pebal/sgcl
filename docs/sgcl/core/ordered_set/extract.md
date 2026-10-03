[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::extract

```cpp
node_type extract(const_iterator pos) noexcept;                            // (1)
node_type extract(const key_type& key) noexcept;                           // (2)
template<class K> node_type extract(K&& key) noexcept(/* see below */);    // (3)
```

Unlinks a node from the chain and from the order and hands it over in a
[node handle](../ordered_set-node_type.md), the element untouched: the handle owns it now, and destroys it if it
dies without having been inserted. Out of the set, the element may change (`value()` of the handle is
writable): this is the way to change an element, which an iterator only reads. [insert](insert.md) links the
node again, here or in another `ordered_set` with the same `Key`, at the end of the order.

1. The node at `pos`.
2. The node of the element equal to `key`; an empty handle when there is none.
3. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to extract; not `end()` |
| `key` | the element to extract |

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
on a stack or inside a managed object, as the set does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<string> s = {"a", "b", "c"};
    auto nh = s.extract("a");
    println("{} {}", s, nh.value());

    nh.value() = nh.value() + "!";  // the element may change outside a set
    s.insert(std::move(nh));  // at the end of the order; no string copied
    println("{} {}", s, nh.empty());

    println("{}", s.extract("z").empty());
}
```

Output:

```text
{"b", "c"} a
{"b", "c", "a!"} true
true
```

## See also

- [node_type](../ordered_set-node_type.md): the node handle
- [insert](insert.md): links the node of a handle
- [merge](merge.md): relinks the nodes of another set
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
