[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](../weak_map.md)

# sgcl::concurrent::weak_map\<Key, T\>::insert

```cpp
pair<iterator, bool> insert(const key_pointer& object, const T& value)    // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
pair<iterator, bool> insert(const key_pointer& object, T&& value)         // (2)
    noexcept(std::is_nothrow_move_constructible_v<T>);
```

Inserts a value for `object`, unless the object has an entry.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

The call is [try_emplace](try_emplace.md) with `value`: one search, and nothing copied or moved when the object has
an entry. The object and the value are two arguments, not a pair: the key is the object, held by a weak pointer the
map makes.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to attach the value to |
| `value` | the value to insert |

## Return value

A pair of an iterator to the entry of `object` and `true` when this call inserted it; or the iterator to the entry
already there and `false`.

## Complexity

Constant on average, plus, every so many insertions, a [sweep](sweep.md) linear in the number of entries: amortized
constant, as [try_emplace](try_emplace.md).

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept.

When the copy or the move of `T` throws, nothing is linked and the map is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of two threads inserting the same object,
exactly one gets `true`. Every insertion counts towards the next sweep, which the inserting thread may run, as in
[try_emplace](try_emplace.md).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    concurrent::weak_map<Node, string> names;
    tracked_ptr node = make_tracked<Node>(1);
    string name = "first";

    auto [it, added] = names.insert(node, name);
    println("{} {}", added, it->value);

    added = names.insert(node, string("second")).second;
    println("{} {}", added, names.find(node)->value);
}
```

Output:

```text
true first
false first
```

## See also

- [try_emplace](try_emplace.md): constructs the value in place
- [emplace](emplace.md): the same as `try_emplace`
- [sgcl::concurrent::weak_map\<Key, T\>](../weak_map.md)
