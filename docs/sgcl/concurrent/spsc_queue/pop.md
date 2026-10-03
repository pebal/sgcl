[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](../spsc_queue.md)

# sgcl::concurrent::spsc_queue\<T\>::pop

```cpp
T pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

The consumer's. Takes the first element, waiting while the queue is empty. The pop tries as
[try_pop](try_pop.md) does; when it finds nothing, it waits on the cell at the head until the push that publishes
it notifies, and then tries again.

## Parameters

None.

## Return value

The first element, moved out.

## Complexity

Constant once there is an element, as `try_pop`; the wait is as long as the queue stays empty.

## Exceptions

What the move constructor of `T` throws; none when it is noexcept. A throw of the move out of the cell leaves the element there, as in
`try_pop`; a throw of the move out of the `optional` into the value returned loses it.

## Notes

Called by the consumer alone, one thread at a time. The wait is a spin of 1024 pause instructions first (about
10 µs on arm64, 40 on x86), since a consumer that has just caught up with the producer is nanoseconds from the
next element, and a wait in the kernel costs both sides a system call; then the atomic's `wait` on the cell's
sequence, the consumer counted among the waiting before it, so that a push publishing the cell after its last
look sees the count and wakes it, and no element is missed.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    concurrent::spsc_queue<string> mailbox(16);

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
- [push](push.md): the producer's side, waiting for room
- [sgcl::concurrent::spsc_queue\<T\>](../spsc_queue.md)
