[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](../bounded_queue.md)

# sgcl::concurrent::bounded_queue\<T\>::pop

```cpp
T pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

Takes the first element, waiting while the queue is empty. The pop tries as [try_pop](try_pop.md) does; when it
finds nothing, it waits on the sequence of the cell at the dequeue position, which the producer publishing that
cell notifies, and then tries again.

## Parameters

None.

## Return value

The first element, moved out.

## Complexity

Constant once there is an element, as `try_pop`; the wait is as long as the queue stays empty.

## Exceptions

What the move constructor of `T` throws, with the element lost as in `try_pop`; none when it is noexcept.

## Notes

The wait is a spin of 1024 pause instructions first (about 10 µs on arm64, 40 on x86), since a consumer that has
just caught up with the producers is nanoseconds from the next element, and a wait in the kernel costs both sides
a system call; then the atomic's `wait` on the cell's sequence, the thread counted among the waiting before it,
so that a producer publishing the cell after the thread's last look sees the count and wakes it, and no element
is missed. The wait is on the sequence the try saw, not on the one the pop wants: the cell may still be waiting
for the consumer of the previous lap to release it. Every other operation of the queue but [push](push.md)
never waits.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    concurrent::bounded_queue<string> mailbox(16);

    thread consumer([&mailbox] {
        string first = mailbox.pop();  // waits until the producer pushes
        string second = mailbox.pop();
        println("{} {}", first, second);
    });

    this_thread::sleep_for(10ms);
    mailbox.push("hello");
    mailbox.push("world");
    consumer.join();
}
```

Output:

```text
hello world
```

## See also

- [try_pop](try_pop.md): returns at once when the queue is empty
- [push](push.md): appends an element, waiting for room
- [sgcl::concurrent::bounded_queue\<T\>](../bounded_queue.md)
