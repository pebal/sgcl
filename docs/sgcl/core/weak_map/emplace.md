[sgcl](../../README.md) › [core](../README.md) › [weak_map](README.md)

# sgcl::weak_map\<Key, T\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(const key_pointer& object, A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A&&...>);
```

Inserts a value for `object`, constructed in place as `T(std::forward<A>(a)...)`, unless the object has an entry.
The table is searched once: when the object has an entry, nothing is built, neither the value nor the weak
pointer, and `a` is left untouched, so a hit costs a lookup; when it has none, the entry is made with the object's
weak pointer and the value built in it, where the search found its place.

Unlike `std::unordered_map::emplace`, which builds the element first and drops it when the key is taken, it is the
`try_emplace` of `std`: the key is the object, given apart from the value, so the search needs nothing built.

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

Constant on average, plus, when the insertion brings the count since the last sweep past the threshold, a
[sweep](sweep.md), linear in the number of entries: amortized constant, as the threshold is the number of entries
the map had after the last sweep, 16 at least.

## Exceptions

What the constructor of `T` throws; none when it is noexcept.

When the constructor of `T` throws, nothing is inserted and the map is as it was.

## Notes

Every entry added counts towards the next sweep, which the insertion that brings the count past the threshold runs
before it returns.

## Example

```cpp
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
    weak_map<Node, Position> positions;
    tracked_ptr node = make_tracked<Node>(1);

    auto [it, added] = positions.emplace(node, 1.5, 2.0);
    println("{} ({}, {})", added, it->value.x, it->value.y);

    added = positions.emplace(node, 0.0, 0.0).second;  // node has a position: nothing built
    println("{} ({}, {})", added, it->value.x, it->value.y);

    weak_map<Node, string> names;
    names.emplace(node, 3, 'a');  // string(3, 'a')
    println("{}", names[node]);
}
```

Output:

```text
true (1.5, 2)
false (1.5, 2)
aaa
```

## See also

- [insert](insert.md): inserts a copy or a moved value
- [insert_or_assign](insert_or_assign.md): assigns the value when the object has one
- [operator[]](operator_at.md): the value of an object, made when it has none
- [sgcl::weak_map\<Key, T\>](README.md)
