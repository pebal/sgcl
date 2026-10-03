[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(const key_pointer& object, A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Inserts a value for `object`, constructed in place from `a...`, unless the object has an entry: the same call as
[try_emplace](try_emplace.md). Unlike `std::unordered_map::emplace` and `concurrent::map::emplace`, which build the
element first and drop it when the key is taken, it searches first and builds nothing when the object has an entry:
the key is the object, given apart from the value, so the search needs nothing built.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to attach the value to |
| `a` | the arguments the value is constructed from |

## Return value

A pair of an iterator to the entry of `object` and `true` when this call inserted it; or the iterator to the entry
already there and `false`.

## Complexity

Constant on average, plus, every so many insertions, a [sweep](sweep.md) linear in the number of entries: amortized
constant, as [try_emplace](try_emplace.md).

## Exceptions

What the constructor of `T` throws; none when it is noexcept.

When the constructor of `T` throws, nothing is linked and the map is as it was.

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

struct Position {
    double x, y;
};

int main() {
    concurrent::weak_map<Node, Position> positions;
    tracked_ptr node = make_tracked<Node>(1);

    auto [it, added] = positions.emplace(node, 1.5, 2.0);
    println("{} ({}, {})", added, it->value.x, it->value.y);

    added = positions.emplace(node, 0.0, 0.0).second;  // node has a position: nothing built
    println("{} ({}, {})", added, it->value.x, it->value.y);
}
```

Output:

```text
true (1.5, 2)
false (1.5, 2)
```

## See also

- [try_emplace](try_emplace.md): the same insertion, and its account
- [insert](insert.md): inserts a copy or a moved value
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)
