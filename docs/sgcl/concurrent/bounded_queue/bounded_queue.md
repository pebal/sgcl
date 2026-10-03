[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](README.md)

# sgcl::concurrent::bounded_queue\<T\>::bounded_queue

```cpp
explicit bounded_queue(size_type capacity) noexcept;    // (1)
bounded_queue(const bounded_queue&) = delete;           // (2)
```

1. An empty queue of `capacity` cells, rounded up to a power of two, at least two: one managed buffer of that many
   cells, every cell free, its sequence numbered from zero.
2. The queue is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

| Parameter | Description |
|---|---|
| `capacity` | the number of elements the queue holds at most, before the rounding |

## Complexity

Linear in the capacity: one allocation, and a store of the sequence of every cell.

## Exceptions

None.

## Notes

The capacity is at least two because in a ring of one cell a cell published at a position and free at the next would
carry one number. The buffer is raw storage: no element is constructed in it until a push, and it is zeroed when the
element may hold tracked pointers, so that the collector, which traces every cell, finds null pointers in the cells
without an element. A capacity whose buffer the managed heap cannot give, up to `SIZE_MAX`, ends the program as any
refused managed allocation does ([collector](../../core/collector/README.md#the-memory-limit)): a capacity past the largest
ring an address space holds is taken as that ring, refused in the same way.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Server {
    concurrent::bounded_queue<string> requests{1000};  // a member of a managed object
};

int main() {
    concurrent::bounded_queue<int> numbers(5);  // on the stack
    tracked_ptr server = make_tracked<Server>();
    concurrent::bounded_queue<int> one(1);

    println("{} {} {}", numbers.capacity(), server->requests.capacity(), one.capacity());
    println("{}", std::is_copy_constructible_v<concurrent::bounded_queue<int>>);
}
```

Output:

```text
8 1024 2
false
```

## See also

- [capacity](capacity.md): the number of cells
- [try_push](try_push.md), [push](push.md): append an element
- [sgcl::concurrent::bounded_queue\<T\>](README.md)
