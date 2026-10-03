[sgcl](../../README.md) › [core](../README.md) › [collector](README.md)

# sgcl::collector::force_collect

```cpp
static bool force_collect(bool wait = false) noexcept;
```

Requests a full collection. With `wait == false` it wakes the collector and returns `true` at once; the cycle runs
concurrently. With `wait == true` it returns once a full cycle that started after the call has completed and found
nothing more to remove: the current cycle may be half done, and a destructor that stores a pointer keeps its target
for one more cycle, so several cycles may run (a bounded number, since a mutator that keeps allocating produces
garbage forever). Zeroes the unused stack below the caller's frame first.

## Parameters

| Parameter | Description |
|---|---|
| `wait` | whether to wait for the collection to complete |

## Return value

`true`; `false` only when the collector is terminating and no such cycle will come (after
[terminate](terminate.md)).

## Complexity

With `wait`, the time of the cycles until nothing more dies; without, constant.

## Exceptions

None.

## Notes

Optional: the collector runs its cycles by itself, and a program that calls it in a loop only wastes CPU on cycles
that would have run anyway. With `wait == true` it blocks the caller, so a destructor of a managed object, which
runs inside a cycle, does not call it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Item {
    int value = 0;
};

// The stack is scanned conservatively: the first item's address is
// overwritten by the next ones, in a frame the collection zeroes.
static weak_ptr<Item> make_and_drop() {
    tracked_ptr item = make_tracked<Item>();
    weak_ptr<Item> first = item;
    for (int i : range(100)) {
        item = make_tracked<Item>(i);
    }
    return first;
}

int main() {
    weak_ptr<Item> weak = make_and_drop();
    // optional, for the demonstration only: the next cycle clears it anyway
    println("{}", collector::force_collect(true));
    println("{}", weak.expired());
}
```

Output:

```text
true
true
```

## See also

- [get_live_object_count](get_live_object_count.md): a count after a full cycle
- [clear_stack](clear_stack.md): the zeroing alone
- [sgcl::collector](README.md)
