[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::operator[]

```cpp
T& operator[](const key_pointer& object) noexcept(std::is_nothrow_default_constructible_v<T>);
```

Returns a reference to the value of `object`, a `T()` made in the entry if the object has none. One search: when
it finds nothing, the entry is built in place from the pointer, as [emplace](emplace.md) builds it.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose value to return |

## Return value

A reference to the value of `object`. It stays valid while the entry is in the map, and is invalid once the entry
is erased or swept.

## Complexity

Constant on average, plus, when an entry is added, the sweep every so many insertions: amortized constant, as
[emplace](emplace.md).

## Exceptions

What the default constructor of `T` throws; none when it is noexcept.

When it throws, nothing is inserted and the map is as it was.

## Notes

An entry added counts towards the next sweep, as one added by [emplace](emplace.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    weak_map<Node, int> visits;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    visits[a] += 1;
    visits[a] += 1;
    visits[b] += 1;
    println("{} {} {}", visits[a], visits[b], visits.size());

    weak_map<Node, string> names;
    names[a] = "first";
    println("\"{}\" \"{}\"", names[a], names[b]);
}
```

Output:

```text
2 1 2
"first" ""
```

## See also

- [emplace](emplace.md): constructs a value from arguments, unless the object has one
- [insert_or_assign](insert_or_assign.md): inserts or assigns a value
- [find](find.md): the entry of an object, without adding one
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
