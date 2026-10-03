[sgcl](../../README.md) › [core](../README.md) › [collector](README.md)

# sgcl::collector::get_referrers

```cpp
static std::tuple<pause_guard, std::vector<referrer>> get_referrers(const void* p) noexcept;
```

Every word that points at the object `p` points into, as a [referrer](../collector-referrer.md) each:

- the members of objects (`object`, with the holder's type and the word's offset);
- the elements of buffers (`buffer`, the element type as `typeid(T[])`, the offset from the buffer's start, header
  included);
- the cells of [root_ptr](../root_ptr/README.md)s in unmanaged memory (`cell`: the block and the cell's offset; the
  `root_ptr` that owns the cell is not known to the collector);
- the words of every thread's stack (`stack`: the word's address and the thread's id; on the calling thread, the
  frames above the call, so a local that holds the object is listed);
- the object itself when a `unique_ptr` owns it (`unique`);
- the cells of `weak_ptr`s (`weak`), which hold nothing.

A full cycle runs first, and the collector stays paused while the `pause_guard` lives, as with
[get_live_objects](get_live_objects.md), so that the live objects are exactly the marked ones and no page moves
under the walk. The mutators run on, and a word is read as the scan reads it.

## Parameters

| Parameter | Description |
|---|---|
| `p` | a pointer to the object or into it |

## Return value

The guard of the pause and the referrers, in a `std::tuple`; no referrer when `p` is not into a live managed
object.

## Complexity

A full cycle, then a walk over the live objects and the stacks.

## Exceptions

None.

## Notes

The stacks are scanned conservatively, so a `stack` referrer may be a word a dead frame left behind as well as a
live local. One guard at a time: a call made while a guard lives waits for a cycle the paused collector cannot run.

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
    weak_ptr<Leaf> watch = head->next->leaf;

    auto [guard, referrers] = collector::get_referrers(head->next->leaf.get());
    int members = 0, weak_cells = 0;
    for (auto& r : referrers) {
        if (r.from == collector::referrer::kind::object) {
            members += r.holder == head->next.get() && r.offset == 8;  // the second node's leaf
        } else if (r.from == collector::referrer::kind::weak) {
            ++weak_cells;
        }
    }
    println("{} member, {} weak cell", members, weak_cells);
}
```

Output:

```text
1 member, 1 weak cell
```

## See also

- [get_path_to_root](get_path_to_root.md): one chain up to a root
- [referrer](../collector-referrer.md): the fields of a referrer
- [sgcl::collector](README.md)
