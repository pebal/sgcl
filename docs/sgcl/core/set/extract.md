[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::extract

```cpp
node_type extract(const_iterator pos) noexcept;                            // (1)
node_type extract(const key_type& key) noexcept;                           // (2)
template<class K> node_type extract(K&& key) noexcept(/* see below */);    // (3)
```

Unlinks a node and hands it over in a [node handle](../set-node_type.md), the element untouched: neither copied
nor destroyed. The handle destroys the element if it dies without having been inserted anywhere.

1. Extracts the node at `pos`.
2. Extracts the node with the key `key`, if there is one.
3. The same with a key of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual`
   both declare `is_transparent`, and `K` converts to neither `iterator` nor `const_iterator`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to extract |
| `key` | the key of the element to extract |

## Return value

A handle holding the node; an empty handle when `pos` is `end()` (1) or no element has the key (2–3).

## Complexity

Constant on average, the walk of one bucket.

## Exceptions

- (1–2) None.
- (3) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are std's function objects;
  otherwise what they throw.

## Notes

This is the way to change a key: outside a set, [value()](../set-node_type/value.md) of the handle is writable,
and [insert](insert.md) of the handle links the node again, hashed anew, with no copy of the element. A node
extracted from a set may be inserted into a [multiset](../multiset.md) of the same `Key`, and back: their
handles are one type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<string> s = {"a", "b"};
    auto nh = s.extract("a");
    println("{} {}", nh.value(), s.size());

    nh.value() = nh.value() + "!";  // the key may change outside a set
    s.insert(std::move(nh));
    println("{} {} {}", s.contains("a!"), s.contains("a"), nh.empty());

    println("{}", s.extract("zzz").empty());
}
```

Output:

```text
a 1
true false true
true
```

## See also

- [insert](insert.md): links the node of a handle
- [node_type](../set-node_type.md): the node handle
- [erase](erase.md): erases an element, destroying it
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
