[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](README.md)

# sgcl::weak_multimap\<Key, T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of entries, the dead ones not yet swept included: the table's count, which a sweep brings down
to the entries of the live objects.

## Parameters

None.

## Return value

The number of entries, live and dead.

## Complexity

Constant.

## Exceptions

None.

## Notes

The number is exact for the live objects right after a [sweep](sweep.md); between the sweeps it counts too the
entries whose objects have gone since. The number of live entries is what a walk from [begin](begin.md) counts.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

void tag_a_temporary(weak_multimap<Node, string>& tags) {
    tracked_ptr node = make_tracked<Node>(2);
    tags.insert(node, "x");
    tags.insert(node, "y");
}

int main() {
    weak_multimap<Node, string> tags;
    tracked_ptr kept = make_tracked<Node>(1);
    tags.insert(kept, "z");
    tag_a_temporary(tags);
    println("{}", tags.size());

    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the node
    collector::force_collect(true);  // optional, for the demonstration
    println("{}", tags.size());

    tags.sweep();
    println("{}", tags.size());
}
```

Output:

```text
3
3
1
```

## See also

- [empty](empty.md): checks whether the map holds an entry
- [count](count.md): the number of entries of one object
- [sweep](sweep.md): erases the dead entries
- [sgcl::weak_multimap\<Key, T\>](README.md)
