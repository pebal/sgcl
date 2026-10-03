[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::set

```cpp
map set(const Key& key, const T& value) const                                       // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, const T&>);
map set(const Key& key, T&& value) const                                            // (2)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, T&&>);
map set(Key&& key, const T& value) const                                            // (3)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, const T&>);
map set(Key&& key, T&& value) const                                                 // (4)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, T&&>);
```

Returns the map with `value` under `key`: added when the key is absent, in place of the value there when it is
present. It is the `set` of [vector](../vector/set.md) and the `insert_or_assign` of the mutable maps. This map is
unchanged. The nodes on the path to the element are copied, log32(*n*) of them, and the rest is shared.

- (1–4) The element is made of `key` and `value`, each copied or moved as it is passed.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |
| `value` | the value to put under the key |

## Return value

The new map: one element more when the key was absent, the same size when it was there.

## Complexity

Logarithmic in `size()`, base 32: the nodes on the path copied, up to 32 elements each.

## Exceptions

What the copy or the move of `Key` and `T` into the element, and the copy of the elements on the path, throw;
none when they are noexcept.

This map is never changed, so an exception leaves it as it was; no new map is made.

## Notes

750 ns per `set` replacing the values of a map of a hundred thousand `int`s; over 200,000 random `long` keys,
480 ns a version each, against 452 ns for immer's map ([Benchmarks](../benchmarks.md#against-immer)). The links
of the copied nodes are taken without the write barrier and each source node shaded once
([tracked_ptr: shade](../../core/tracked_ptr/shade.md)); the elements are copied as any copy,
which is where the time goes.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> ports = {{"http", 80}};
    auto changed = ports.set("http", 8080);
    auto added = changed.set("ssh", 22);
    println("{} {} {}", ports.at("http"), changed.at("http"), added.size());
}
```

Output:

```text
80 8080 2
```

## See also

- [insert](insert.md): the map with an element added only when its key is absent
- [update](update.md): the map with a function of the value in its place
- [erase](erase.md): the map without the element under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
