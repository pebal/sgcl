[sgcl](../../README.md) › [immutable](../README.md) › [map](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::insert

```cpp
map insert(const Key& key, const T& value) const                                    // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, const T&>);
map insert(const Key& key, T&& value) const                                         // (2)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, T&&>);
map insert(Key&& key, const T& value) const                                         // (3)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, const T&>);
map insert(Key&& key, T&& value) const                                              // (4)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, T&&>);
map insert(const value_type& value) const                                           // (5)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, const T&>);
```

Returns the map with `value` under `key` when the key is absent, and this same map, sharing everything, when the
key is there: as every `insert` of the library, it keeps what it finds. This map is unchanged. The nodes on the
path to the element are copied, log32(*n*) of them, and the rest is shared.

- (1–4) The element is made of `key` and `value`, each copied or moved as it is passed.
- (5) The element is a copy of the pair `value`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |
| `value` | the value of the element; (5) the element, a pair of the key and the value |

## Return value

The new map, one element more; this map when the key was there.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then the nodes on the path copied, up to 32 elements each.

## Exceptions

What the copy or the move of `Key` and `T` into the element, and the copy of the elements on the path, throw;
none when they are noexcept.

This map is never changed, so an exception leaves it as it was; no new map is made.

## Notes

[set](set.md) is the one that replaces a value that is there. 590 ns per insert while building a hundred thousand
`int`s one version at a time; a map built from a range at once, or through a [builder](thaw.md), costs a fraction
of that ([Benchmarks](../benchmarks.md#against-std)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> ports = {{"http", 80}};
    auto more = ports.insert("https", 443);
    auto kept = more.insert("http", 8080);  // http is there: the same map
    println("{} {} {}", ports.size(), more.size(), kept.at("http"));
}
```

Output:

```text
1 2 80
```

## See also

- [set](set.md): the map with a value added or in place of the one there
- [emplace](emplace.md): the value constructed in place
- [erase](erase.md): the map without the element under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](README.md)
