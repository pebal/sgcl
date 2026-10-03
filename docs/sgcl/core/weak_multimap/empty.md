[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](README.md)

# sgcl::weak_multimap\<Key, T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map holds an entry. An entry whose object is gone and that no sweep has dropped yet is an entry:
`empty` answers for the table, as [size](size.md) counts it, not for the live objects.

## Parameters

None.

## Return value

`true` when the map holds no entry, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

The answer is exact for the live objects right after a [sweep](sweep.md). Whether the map has a live entry is what
[begin](begin.md) answers: `begin() == end()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

void tag_a_temporary(weak_multimap<Node, string>& tags) {
    tracked_ptr node = make_tracked<Node>(1);
    tags.insert(node, "x");
    tags.insert(node, "y");
}

int main() {
    weak_multimap<Node, string> tags;
    println("{}", tags.empty());

    tag_a_temporary(tags);
    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the node
    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", tags.empty(), tags.begin() == tags.end());

    tags.sweep();
    println("{}", tags.empty());
}
```

Output:

```text
true
false true
true
```

## See also

- [size](size.md): the number of entries
- [sweep](sweep.md): erases the dead entries
- [sgcl::weak_multimap\<Key, T\>](README.md)
