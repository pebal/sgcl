[sgcl](../../README.md) › [core](../README.md) › [weak_map](README.md)

# sgcl::weak_map\<Key, T\>::insert

```cpp
pair<iterator, bool> insert(const key_pointer& object, const T& value)    // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
pair<iterator, bool> insert(const key_pointer& object, T&& value)         // (2)
    noexcept(std::is_nothrow_move_constructible_v<T>);
```

Inserts a value for `object`, unless the object has an entry.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

The call is [emplace](emplace.md) with `value`: one search, and nothing copied or moved when the object has an
entry. The object and the value are two arguments, not a pair: the key is the object, held by a weak pointer the
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

Constant on average, plus, every so many insertions, a [sweep](sweep.md) linear in the number of entries:
amortized constant, as [emplace](emplace.md).

## Exceptions

What the copy (1) or the move (2) constructor of `T` throws; none when it is noexcept.

When the copy or the move of `T` throws, nothing is inserted and the map is as it was.

## Notes

Every entry added counts towards the next sweep: the insertion that brings the count since the last sweep past the
number of entries the map had after it, 16 at least, sweeps the dead entries out before it returns.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

void name_temporaries(weak_map<Node, string>& names) {
    for (int i : range(10)) {
        tracked_ptr node = make_tracked<Node>(100 + i);
        names.insert(node, "temporary");
    }
}

int main() {
    weak_map<Node, string> names;
    tracked_ptr node = make_tracked<Node>(1);
    string name = "first";
    auto [it, added] = names.insert(node, name);
    println("{} {}", added, it->value);

    added = names.insert(node, string("second")).second;
    println("{} {}", added, names.find(node)->value);

    name_temporaries(names);
    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the nodes
    collector::force_collect(true);  // optional, for the demonstration
    println("{} entries", names.size());

    vector<tracked_ptr<Node>> kept;
    for (int i : range(6)) {
        kept.push_back(make_tracked<Node>(200 + i));
        names.insert(kept.back(), "kept");  // the seventeenth insertion sweeps
    }
    println("{} entries", names.size());
}
```

Output:

```text
true first
false first
11 entries
7 entries
```

## See also

- [emplace](emplace.md): constructs the value in place
- [insert_or_assign](insert_or_assign.md): assigns the value when the object has one
- [operator[]](operator_at.md): the value of an object, made when it has none
- [sgcl::weak_map\<Key, T\>](README.md)
