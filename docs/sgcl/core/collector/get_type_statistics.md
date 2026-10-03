[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::get_type_statistics

```cpp
static std::vector<type_statistics> get_type_statistics() noexcept;
```

The live objects by type after a full cycle ([type_statistics](../collector-type_statistics.md)): what a heap that
grows is made of. Objects are listed by their type; the buffers of the containers (`vector`, `dynamic_array<T>`, the
maps of `deque`, the buckets of the hash tables) by their array type, `typeid(T[])` for elements `T`, with
`buffers == true`, the slot they occupy as their bytes and no pages, since the pages of buffers belong to size
classes rather than to a type. Sorted by `live_bytes`, descending, then by `live_objects`.

Like [get_live_objects](get_live_objects.md): a full cycle runs first, the caller's dead frames are zeroed, the
caller waits for the cycle.

## Parameters

None.

## Return value

One [type_statistics](../collector-type_statistics.md) per type with live objects, in a `std::vector`.

## Complexity

A full cycle, then linear in the number of types.

## Exceptions

None.

## Notes

The list is a `std::vector`, made while the collector is paused, when an allocation on the managed heap could wait
for the cycle the pause holds back. The function is declared always-inline, so that no frame of its own lies
between the caller and the stack it zeroes. It blocks the caller, so a destructor of a managed object does not
call it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    tracked_ptr<Node> next;
    int value = 0;
};

int main() {
    vector<tracked_ptr<Node>> kept;
    for (int i : range(10)) {
        kept.push_back(make_tracked<Node>());
    }
    auto types = collector::get_type_statistics();
    for (auto& t : types) {
        if (*t.type == typeid(Node)) {
            println("Node: {} x {} B = {} B, {} page", t.live_objects, t.object_size, t.live_bytes,
                    t.pages);
        }
    }
    for (auto& t : types) {
        if (*t.type == typeid(tracked_ptr<Node>[])) {
            println("buffers of tracked_ptr<Node>: {}, {} B each element", t.live_objects,
                    t.object_size);
        }
    }
}
```

Output:

```text
Node: 10 x 16 B = 160 B, 1 page
buffers of tracked_ptr<Node>: 1, 8 B each element
```

## See also

- [type_statistics](../collector-type_statistics.md): the fields
- [get_statistics](get_statistics.md): the counters of the collector's work
- [sgcl::collector](../collector.md)
