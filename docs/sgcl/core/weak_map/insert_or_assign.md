[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::insert_or_assign

```cpp
template<class V>
pair<iterator, bool> insert_or_assign(const key_pointer& object, V&& value)
    noexcept(std::is_nothrow_constructible_v<T, V&&> && std::is_nothrow_assignable_v<T&, V&&>);
```

Inserts a value for `object` constructed from `value`, or, when the object has an entry, assigns `value` to the
value there, replacing it. One search, as [emplace](emplace.md): the value is built in the entry only when the
object has none.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to attach the value to |
| `value` | the value to insert or assign |

## Return value

A pair of an iterator to the entry of `object` and `true` when this call inserted it; or the iterator to the entry
already there, its value assigned, and `false`.

## Complexity

Constant on average, plus, every so many insertions, a [sweep](sweep.md) linear in the number of entries:
amortized constant, as [emplace](emplace.md).

## Exceptions

What the construction of `T` from `value`, or its assignment, throws; none when both are noexcept.

When the construction throws, nothing is inserted and the map is as it was; when the assignment throws, the entry
stays, its value as the assignment of `T` left it.

## Notes

Every entry added counts towards the next sweep; an assignment does not.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    weak_map<Node, string> names;
    tracked_ptr node = make_tracked<Node>(1);

    auto [it, added] = names.emplace(node, "one");
    println("{} {}", added, it->value);

    added = names.insert_or_assign(node, "uno").second;
    println("{} {}", added, names[node]);
}
```

Output:

```text
true one
false uno
```

## See also

- [insert](insert.md): inserts a value, unless the object has one
- [operator[]](operator_at.md): the value of an object, made when it has none
- [sgcl::weak_map\<Key, T\>](../weak_map.md)
