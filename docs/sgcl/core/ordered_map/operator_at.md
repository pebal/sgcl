[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::operator[]

```cpp
mapped_type& operator[](const key_type& key)                 // (1)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_default_constructible_v<T>);
mapped_type& operator[](key_type&& key)                      // (2)
    noexcept(std::is_nothrow_move_constructible_v<Key> &&
             std::is_nothrow_default_constructible_v<T>);
```

Returns a reference to the value under `key`, inserting it first when the key is absent: a new element at the
end of the order, its value value-initialized in place, as `try_emplace(key)` does. A key that
is there keeps its value and its place. For a `tracked_ptr` value the new value is a null pointer.

1. Copies `key` into a new element.
2. Moves `key` into a new element; a key that is there is not moved from.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

A reference to the value under the key.

## Complexity

Constant on average, linear in `size()` in the worst case; amortized over the growths of the table.

## Exceptions

What the copy or the move of `Key` and the default constructor of `T` throw; none when they are noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> counts;
    for (auto word : {"to", "be", "or", "not", "to", "be"}) {
        ++counts[word];  // inserted as 0 the first time
    }
    println("{}", counts);

    ordered_map<int, tracked_ptr<int>> slots;
    println("{}", slots[7] == nullptr);
}
```

Output:

```text
{"to": 2, "be": 2, "or": 1, "not": 1}
true
```

## See also

- [at](at.md): the value under a key, which must be there
- [try_emplace](try_emplace.md): inserts a value built from arguments when the key is absent
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
