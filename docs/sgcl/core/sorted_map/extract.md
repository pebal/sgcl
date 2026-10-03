[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::extract

```cpp
node_type extract(iterator pos) noexcept                                   // (1)
    requires (!std::is_same_v<iterator, const_iterator>);
node_type extract(const_iterator pos) noexcept;                            // (2)
node_type extract(const key_type& key) noexcept;                           // (3)
template<class K> node_type extract(K&& key) noexcept(/* see below */);    // (4)
```

Unlinks a node and hands it over in a [node handle](../sorted_map-node_type/README.md), the element untouched: the handle
owns it now, its key may be changed there, and it destroys the element if it dies without having been inserted.
The node goes back into a map, this one or another with the same `Key` and `T`, through
[insert](insert.md) with no copy.

1. Extracts the element `pos` addresses, which must be an element of this map, not `end()`. The clause keeps the
   overload apart from (2) where the two iterators are one type (the sets'); in a map they differ.
2. The same with a `const_iterator`.
3. Extracts the element under `key`; returns an empty handle when there is none.
4. As (3), with a key of another type, compared without building a `key_type`. Takes part only when `Compare`
   declares `is_transparent`, as `std::less` of a [string](../string/README.md) does, and `K` converts to neither
   `iterator` nor `const_iterator`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to extract |
| `key` | the key of the element to extract |

## Return value

A node handle that owns the extracted element, or an empty handle.

## Complexity

- (1–2) Amortized constant.
- (3–4) Logarithmic in the size of the map.

## Exceptions

- (1–3) None.
- (4) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a
  function object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<int, string> m = {{1, "a"}, {2, "b"}};
    auto nh = m.extract(1);
    println("{} {} {}", m, nh.key(), nh.mapped());

    nh.key() = 7;  // the key may change outside a map
    m.insert(std::move(nh));  // no string is copied
    println("{}", m);

    println("{}", m.extract(5).empty());
}
```

Output:

```text
{2: "b"} 1 a
{2: "b", 7: "a"}
true
```

## See also

- [insert](insert.md): links a node handle's node back into a map
- [merge](merge.md): relinks the nodes of another map into this one
- [take](take.md): moves the value under a key out and erases the element
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
