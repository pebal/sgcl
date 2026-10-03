[sgcl](../../README.md) › [concurrent](../README.md) › [queue](../queue.md)

# sgcl::concurrent::queue\<T\>::pop

```cpp
T pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

Takes the first element, waiting while the queue is empty. The pop tries as [try_pop](try_pop.md) does; when it
finds nothing, it counts itself among the waiting, looks once more, and waits on the link of the last node, which
the push that links after it notifies; then it tries again.

## Parameters

None.

## Return value

The first element, moved out.

## Complexity

Constant once there is an element, as `try_pop`; the wait is as long as the queue stays empty.

## Exceptions

What the move constructor of `T` throws, with the element lost as in `try_pop`; none when it is noexcept.

## Notes

The wait is the atomic's `wait` on the link of the last node, a wait in the kernel; the thread counts itself
before its last look, so a push that links an element after that look sees the count and wakes it, and no element
is missed. Every other operation of the queue is lock-free; `pop` alone waits, and only for an element.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    concurrent::queue<string> mailbox;

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
- [push](push.md): appends an element and wakes a waiting `pop`
- [sgcl::concurrent::queue\<T\>](../queue.md)
