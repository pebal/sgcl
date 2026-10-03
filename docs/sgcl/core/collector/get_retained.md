[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::get_retained

```cpp
static retained get_retained(const void* p) noexcept;
```

What dies with the object `p` points into: the objects reachable from it and from nowhere else, itself included,
and their bytes, the slots they occupy (a container's buffer at the slot of its size class). What is shared with
another root stays out; a weak pointer holds nothing, so what is reachable only through a `weak_ptr` is not
retained by its holder.

A full cycle runs first and the collector is paused for the walk, as with [get_referrers](get_referrers.md); the
pause ends before the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `p` | a pointer to the object or into it |

## Return value

The objects and the bytes, a [retained](../collector-retained.md); `{0, 0}` when `p` is not into a live managed
object.

## Complexity

A full cycle, then a walk over the live objects.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Leaf {
    int value;
};

struct Node {
    tracked_ptr<Node> next;
    tracked_ptr<Leaf> leaf;
};

int main() {
    unique_ptr head = make_tracked<Node>();
    head->next = make_tracked<Node>();
    head->next->leaf = make_tracked<Leaf>(7);
    tracked_ptr shared = make_tracked<Leaf>(8);
    head->leaf = shared;  // held from the stack as well

    auto all = collector::get_retained(head.get());
    auto second = collector::get_retained(head->next.get());
    println("{} objects, {} objects", all.objects, second.objects);

    int local = 0;
    println("{}", collector::get_retained(&local).objects);
}
```

Output:

```text
3 objects, 2 objects
0
```

## See also

- [retained](../collector-retained.md): the fields
- [explain](explain.md): what holds an object and what it retains, as text
- [sgcl::collector](../collector.md)
