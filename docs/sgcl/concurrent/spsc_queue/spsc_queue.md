[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](../spsc_queue.md)

# sgcl::concurrent::spsc_queue\<T\>::spsc_queue

```cpp
/*(1)*/ explicit spsc_queue(size_type capacity) noexcept;
/*(2)*/ spsc_queue(const spsc_queue&) = delete;
```

1. An empty queue of `capacity` cells, rounded up to a power of two, at least one: one managed buffer of that many
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

The buffer is raw storage: no element is constructed in it until a push, and it is zeroed when the element may hold
tracked pointers, so that the collector, which traces every cell, finds null pointers in the cells without an
element. A capacity whose buffer the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused
managed allocation does ([collector](../../core/collector.md#the-memory-limit)): a capacity past the largest ring an
address space holds is taken as that ring, refused in the same way. The producer and the consumer are not fixed at
construction: a side may pass from one thread to another, the two ordered by the program's own synchronization (a
join, a mutex); one thread at a time on each side is the whole rule.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Reader {
    concurrent::spsc_queue<string> lines{1000};  // a member of a managed object
};

int main() {
    concurrent::spsc_queue<int> numbers(5);  // on the stack
    tracked_ptr reader = make_tracked<Reader>();
    concurrent::spsc_queue<int> one(1);

    println("{} {} {}", numbers.capacity(), reader->lines.capacity(), one.capacity());
    println("{}", std::is_copy_constructible_v<concurrent::spsc_queue<int>>);
}
```

Output:

```text
8 1024 1
false
```

## See also

- [capacity](capacity.md): the number of cells
- [try_push](try_push.md), [push](push.md): append an element
- [sgcl::concurrent::spsc_queue\<T\>](../spsc_queue.md)
