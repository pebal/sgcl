[sgcl](../../README.md) › [core](../README.md) › [collector](README.md)

# sgcl::collector::get_live_object_count

```cpp
static size_t get_live_object_count() noexcept;
```

Runs a full collection, waits for it and returns the number of objects it marked: every managed object reachable
from a root, the buffers of the containers included. Zeroes the unused stack below the caller's frame first.

## Parameters

None.

## Return value

The number of live managed objects after the cycle.

## Complexity

A full cycle: the caller is blocked for its length.

## Exceptions

None.

## Notes

The zeroing reaches the frames below the caller's, not the caller's own: a raw pointer or an iterator kept there
retains its target, so code that needs an exact count keeps its pointers in a helper function. The function is
declared always-inline, so that no frame of its own lies between the caller and the stack it zeroes. It blocks the
caller, so a destructor of a managed object does not call it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    tracked_ptr<Node> next;
    int value;
};

// the pointers in a frame of their own: the stack is scanned conservatively
static void build_and_drop() {
    tracked_ptr<Node> head;
    for (int i : range(1000)) {
        head = make_tracked<Node>(head, i);
    }
}  // the list is garbage

int main() {
    size_t before = collector::get_live_object_count();
    build_and_drop();
    // a full cycle first
    println("{} new live objects", collector::get_live_object_count() - before);
}
```

Output:

```text
0 new live objects
```

## See also

- [get_live_objects](get_live_objects.md): the addresses of the live objects
- [get_type_statistics](get_type_statistics.md): the live objects by type
- [sgcl::collector](README.md)
