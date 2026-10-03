[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](README.md)

# sgcl::weak_multimap\<Key, T\>::insert

```cpp
iterator insert(const key_pointer& object, const T& value);    // (1)
iterator insert(const key_pointer& object, T&& value);         // (2)
```

Inserts one more value for `object`, whatever entries the object has already.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

The call is [emplace](emplace.md) with `value`: the value is copied or moved once, into the entry, which goes in
front of the object's others. The object and the value are two arguments, not a pair: the key is the object, held
by a weak pointer the map makes.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to attach the value to |
| `value` | the value to insert |

## Return value

An iterator to the inserted entry.

## Complexity

Constant on average, plus, every so many insertions, a [sweep](sweep.md) linear in the number of entries:
amortized constant, as [emplace](emplace.md).

## Exceptions

What the copy (1) or the move (2) constructor of `T` throws.

When it throws, nothing is inserted and the map is as it was.

## Notes

The call is not `noexcept`, even for a value whose copy or move is, as [emplace](emplace.md) is not. Every
insertion counts towards the next sweep.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    weak_multimap<Node, string> tags;
    tracked_ptr node = make_tracked<Node>(1);
    string tag = "red";
    tags.insert(node, tag);
    auto it = tags.insert(node, string("round"));
    println("{} {}", it->value, tags.count(node));
}
```

Output:

```text
round 2
```

## See also

- [emplace](emplace.md): constructs the value in place
- [equal_range](equal_range.md): the entries of an object
- [sgcl::weak_multimap\<Key, T\>](README.md)
