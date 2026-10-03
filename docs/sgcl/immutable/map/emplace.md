[sgcl](../../README.md) › [immutable](../README.md) › [map](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
map emplace(const Key& key, A&&... a) const
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, A...>);
```

Returns the map with an element under `key` whose value is constructed from `a...` in its place in the new node,
when the key is absent; this same map when the key is there, as [insert](insert.md). This map is unchanged.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element, copied into it |
| `a` | the arguments the value is constructed from |

## Return value

The new map, one element more; this map when the key was there.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then the nodes on the path copied, up to 32 elements each.

## Exceptions

What the copy of `Key`, the constructor of `T` from `a...` and the copy of the other elements throw; none when
they are noexcept.

This map is never changed, so an exception leaves it as it was; no new map is made.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<int, string> lines;
    auto one = lines.emplace(1, 3, '-');  // the value is string(3, '-')
    auto same = one.emplace(1, "ignored");
    println("{} {}", one.at(1), same.at(1));
}
```

Output:

```text
--- ---
```

## See also

- [insert](insert.md): the map with an element added when its key is absent
- [set](set.md): the map with a value added or in place of the one there
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](README.md)
