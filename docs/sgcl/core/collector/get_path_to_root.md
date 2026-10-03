[sgcl](../../README.md) › [core](../README.md) › [collector](README.md)

# sgcl::collector::get_path_to_root

```cpp
static std::tuple<pause_guard, std::vector<referrer>> get_path_to_root(const void* p) noexcept;
```

A chain from the object `p` points into up to a root, as [referrer](../collector-referrer.md)s: `[0]` holds the
object, `[1]` holds that holder, and so on to a root: an object a `unique_ptr` owns (`unique`: `holder` is the
object itself), a block of cells of [root_ptr](../root_ptr/README.md)s in unmanaged memory (`cell`: the block, `offset` the
cell; the block itself follows as a `unique` link, a root by its state), a word on a stack.

The search goes from the roots down, breadth first: the roots by state first, then the other threads' stacks, and
the calling thread's frames above the call only when nothing else reaches the object. The caller holds the pointer
it asks about and asks what else does, so a chain that ends on its own stack says that nothing else does: a local,
or a word a dead frame left, garbage once the frame is gone. The search does not go through a weak cell: a
`weak_ptr` holds nothing, so no chain leads through one ([get_referrers](get_referrers.md) still lists it).

A full cycle runs first, and the collector stays paused while the `pause_guard` lives, as with
[get_live_objects](get_live_objects.md).

## Parameters

| Parameter | Description |
|---|---|
| `p` | a pointer to the object or into it |

## Return value

The guard of the pause and the chain, in a `std::tuple`. The chain is empty when `p` is not into a live managed
object, or when only the frames of the call hold it.

## Complexity

A full cycle, then a breadth-first search over the live objects.

## Exceptions

None.

## Notes

One guard at a time: a call made while a guard lives waits for a cycle the paused collector cannot run.
[explain](explain.md) writes the same chain as text.

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

const char* name_of(collector::referrer::kind kind) {
    switch (kind) {
        case collector::referrer::kind::object: return "a member of an object";
        case collector::referrer::kind::unique: return "a unique_ptr's object";
        default: return "another root";
    }
}

int main() {
    unique_ptr head = make_tracked<Node>();
    head->next = make_tracked<Node>();
    head->next->leaf = make_tracked<Leaf>(7);

    auto [guard, path] = collector::get_path_to_root(head->next->leaf.get());
    for (auto& link : path) {
        println("{}, the word at byte {}", name_of(link.from), link.offset);
    }
}
```

Output:

```text
a member of an object, the word at byte 8
a member of an object, the word at byte 0
a unique_ptr's object, the word at byte 0
```

## See also

- [explain](explain.md): the chain as text
- [get_referrers](get_referrers.md): every word that points at the object
- [sgcl::collector](README.md)
