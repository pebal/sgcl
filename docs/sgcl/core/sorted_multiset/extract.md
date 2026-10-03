[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::extract

```cpp
/*(1)*/ node_type extract(const_iterator pos) noexcept;
/*(2)*/ node_type extract(const key_type& key) noexcept;
/*(3)*/ template<class K> node_type extract(K&& key) noexcept(/* see below */);
```

Unlinks a node from the tree and hands it over in a [node handle](../sorted_set-node_type.md), the element
untouched: the handle owns the element from now on, and destroys it if it dies without the node being inserted
anywhere.

1. Extracts the node of the element at `pos`.
2. Extracts the node of the first element with a key equivalent to `key`, the others staying; an empty handle when
   there is none.
3. As (2), with a key of another type, compared without building a `key_type`. Takes part only when `Compare`
   declares `is_transparent`, as `std::less` of a [string](../string.md) does, and `K` converts to neither
   `iterator` nor `const_iterator`.

Through the handle the key may be changed, since the node is out of any tree, and the node inserted again, here or
into another sorted_multiset or sorted_set of the same `Key`, with no copy of the element.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to extract; not `end()` |
| `key` | the key of the element to extract |

## Return value

A node handle owning the extracted node, or an empty handle.

## Complexity

- (1) Amortized constant.
- (2–3) Logarithmic in the size of the multiset.

## Exceptions

- (1–2) None.
- (3) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a
  function object of `std`.

## Notes

The handle holds its node by a `tracked_ptr` from before the node leaves the tree, so the node is never
unreachable on the way; the handle lives where a `tracked_ptr` may. Iterators to the extracted element are
invalid, as after an erase.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multiset<string> words = {"a", "a", "c"};

    auto nh = words.extract("a");
    println("{} {}", words, nh.value());

    nh.value() = "b";  // the key may change outside a multiset
    words.insert(std::move(nh));
    println("{} {}", words, nh.empty());
}
```

Output:

```text
{"a", "c"} a
{"a", "b", "c"} true
```

## See also

- [insert](insert.md): inserts a node handle
- [merge](merge.md): moves every node of another set or multiset
- [node_type](../sorted_set-node_type.md): the node handle
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
