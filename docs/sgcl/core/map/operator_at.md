[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::operator[]

```cpp
mapped_type& operator[](const key_type& key)                 // (1)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_default_constructible_v<T>);
mapped_type& operator[](key_type&& key)                      // (2)
    noexcept(std::is_nothrow_move_constructible_v<Key> &&
             std::is_nothrow_default_constructible_v<T>);
```

Returns a reference to the value under `key`, inserting a value-initialized one first when the key is not in the
map, as [try_emplace](try_emplace.md)`(key)` does: the value is built in place, in the node, and for a
`tracked_ptr` mapped type it is a null pointer.

1. A new key is copied into the element.
2. A new key is moved into the element.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

A reference to the value under the key.

## Complexity

Constant on average, linear in the size when every key falls into one bucket; a growth relinks every node,
amortized constant.

## Exceptions

What the construction of the key (its copy or its move) and the default constructor of `T` throw; none when they
are noexcept.

If an exception is thrown, nothing is inserted and the map is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> counts;
    ++counts["x"];  // inserted as 0, now 1
    ++counts["x"];
    println("{} {}", counts["x"], counts.size());

    map<int, tracked_ptr<string>> names;
    println("{}", names[7] == nullptr);
    names[7] = make_tracked<string>("seven");
    println("{} {}", *names[7], names.size());
}
```

Output:

```text
2 1
true
seven 1
```

## See also

- [at](at.md): the value under a key, with bounds checking
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert_or_assign](insert_or_assign.md): inserts, or assigns to the value under the key
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
