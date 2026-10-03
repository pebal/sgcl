[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](README.md)

# sgcl::concurrent::spsc_queue\<T\>::push

```cpp
void push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

The producer's. Appends an element at the end of the queue, waiting for room while the queue is full.

1. Appends a copy of `value`.
2. Appends `value`, moved.

The push tries as [try_push](try_push.md) does; when the queue is full, it waits on the cell at the tail until
the pop that frees it notifies, and then tries again. `value` is taken only by the attempt that finds room.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

None.

## Complexity

Constant once there is room, as `try_push`; the wait is as long as the queue stays full.

## Exceptions

What the copy or the move constructor of `T` throws, with the queue as it was; none when it is noexcept.

## Notes

Called by the producer alone, one thread at a time. The wait is a spin of 1024 pause instructions first (about
10 µs on arm64, 40 on x86), since a producer that has just caught up with the consumer is nanoseconds from the
room it needs, and a wait in the kernel costs both sides a system call; then the atomic's `wait` on the cell's
sequence, the producer counted among the waiting before it, so that a pop freeing the cell after its last look
sees the count and wakes it.

There is no emplacing form of `push`: [try_emplace](try_emplace.md) constructs in place without waiting.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::spsc_queue<int> numbers(4);

    thread producer([&numbers] {
        for (int i : range(100)) {
            numbers.push(i);  // waits while the ring of four is full
        }
    });

    int sum = 0;
    for (int i : range(100)) {
        sum += numbers.pop();
    }
    producer.join();
    println("{} {}", sum, numbers.empty());
}
```

Output:

```text
4950 true
```

## See also

- [try_push](try_push.md), [try_emplace](try_emplace.md): return at once when the queue is full
- [pop](pop.md): the consumer's side, waiting for an element
- [sgcl::concurrent::spsc_queue\<T\>](README.md)
