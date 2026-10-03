[sgcl](../../README.md) › [concurrent](../README.md) › [stack](README.md)

# sgcl::concurrent::stack\<T\>::pop

```cpp
T pop() noexcept(std::is_nothrow_move_constructible_v<T>);
```

Takes the top element, waiting while the stack is empty. The pop tries as [try_pop](try_pop.md) does; when it
finds nothing, it counts itself among the waiting and waits on the head while the head is null, which the next
push notifies; then it tries again.

## Parameters

None.

## Return value

The top element, moved out.

## Complexity

Constant once there is an element, as `try_pop`; the wait is as long as the stack stays empty.

## Exceptions

What the move constructor of `T` throws, with the element lost as in `try_pop`; none when it is noexcept.

## Notes

The wait is the atomic's `wait` on the head, a wait in the kernel. The thread counts itself before the wait's own
look at the head, so a push that publishes an element after that look sees the count and wakes it, and no element
is missed; a push wakes one waiting thread, and a woken thread that finds the element taken by another waits
again. Every other operation of the stack is lock-free; `pop` alone waits, and only for an element.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    concurrent::stack<string> mailbox;

    thread consumer([&mailbox] {
        string message = mailbox.pop();  // waits until the producer pushes
        println("{}", message);
    });

    this_thread::sleep_for(10ms);
    mailbox.push("hello");
    consumer.join();
}
```

Output:

```text
hello
```

## See also

- [try_pop](try_pop.md): returns at once when the stack is empty
- [push](push.md): puts an element and wakes a waiting `pop`
- [sgcl::concurrent::stack\<T\>](README.md)
