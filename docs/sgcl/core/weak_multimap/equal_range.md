[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](../weak_multimap.md)

# sgcl::weak_multimap\<Key, T\>::equal_range

```cpp
pair<iterator, iterator> equal_range(const key_pointer& object) noexcept;                      // (1)
pair<const_iterator, const_iterator> equal_range(const key_pointer& object) const noexcept;    // (2)
```

Returns the range of the entries of `object`, `[first, last)`: they stand together in the table, the newest first,
as in [multimap](../multimap.md). The range is a range of its own: a walk from `first` ends with the object's
entries, at `last`, and an [erase](erase.md) through it returns at most `last`. A null pointer has no entries, and
its range is empty.

1. A range through which the values may be written.
2. A range of `const_iterator`s, on a const map.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entries to return |

## Return value

A pair of iterators, the first entry of `object` and the end of its run; both [end](end.md) when `object` has no
entry or is null. A [range](../range.md) is made from the pair, for a range-for.

## Complexity

Constant on average, plus linear in the number of the object's entries.

## Exceptions

None.

## Notes

`last` stands on the entry after the object's run, which may be a dead one. A walk from `first` does not reach it
by following the table: it ends at `last` on the first entry that is not the object's. So a sweep that drops the
entry `last` stands on, the one an insertion runs included, or a growth of the table that puts another entry
after the run, does not let the walk run on into the entries of other objects.

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
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    tags.insert(a, "first");
    tags.insert(b, "other");
    tags.insert(a, "second");
    tags.insert(a, "third");

    for (auto [node, tag] : range(tags.equal_range(a))) {
        println("{}: {}", node->id, tag);
    }
    auto [first, last] = tags.equal_range(nullptr);
    println("{}", first == last);

    const auto& seen = tags;  // (2), on a const map
    println("{}", seen.equal_range(b).first->value);
}
```

Output:

```text
1: third
1: second
1: first
true
other
```

## See also

- [find](find.md): the first entry of an object
- [count](count.md): the number of entries of an object
- [erase](erase.md): erases an entry, up to the end of its range
- [sgcl::weak_multimap\<Key, T\>](../weak_multimap.md)
