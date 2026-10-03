[sgcl](../../README.md) › [concurrent](../README.md) › [map](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::find

```cpp
iterator find(const Key& key) noexcept;                                // (1)
const_iterator find(const Key& key) const noexcept;                    // (2)
template<class K> iterator find(const K& key) noexcept;                // (3)
template<class K> const_iterator find(const K& key) const noexcept;    // (4)
```

Finds the element under `key`: from the dummy node of the key's bucket along the list while the split keys are
less than the key's, then through the elements of the same split key for the key itself, stepping over erased
nodes without touching them.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../../core/string/README.md) do: a
  `string_view` or a literal finds a `string` key with no string made for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the element, or [end()](end.md) when the key is not there.

## Complexity

Constant on average: the walk over the elements of one bucket, one on average at a load factor of one; linear in
the size when every key falls into one bucket.

## Exceptions

None.

## Notes

Wait-free once the key's bucket has its dummy node, and then it writes nothing. A bucket gets its dummy on its
first use after the array grew: from the first insertion into it, or from the first lookup, which makes it as an
insertion does (an allocation and a compare-exchange, lock-free, once per bucket for the life of the array). A
lookup walked from the nearest initialized ancestor's dummy before, writing nothing; with the array doubled late in
a run of insertions and then only looked up, half the buckets and more had no dummy, several levels of them, and
the ancestor's segment, which the split order interleaves with every uninitialized descendant's, was a large part
of the list: a find of 6 ns took 7000, measured with 200,000 keys inserted by 32 threads and then found by one.

The iterator holds the element's node: an element another thread erases after the search found it stays alive and
is read as it was.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    concurrent::map<string, int> ages = {{"Ada", 36}, {"Grace", 85}};

    auto it = ages.find("Grace");  // a literal: no string made
    println("{} {}", it->first, it->second);

    std::string_view name = "Linus";
    println("{}", ages.find(name) == ages.end());

    ages.erase("Grace");
    println("{} {}", it->second, ages.find("Grace") == ages.end());
}
```

Output:

```text
Grace 85
true
85 true
```

## See also

- [contains](contains.md): checks whether a key is there
- [value_or](value_or.md): a copy of the value under a key, or a fallback
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](README.md)
