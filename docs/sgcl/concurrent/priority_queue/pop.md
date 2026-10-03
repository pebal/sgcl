[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](README.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::pop

```cpp
T pop() noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>);
```

Takes the least element, waiting while the queue is empty. The pop reads the count of the pushes, tries as
[try_pop](try_pop.md) does, and when it finds nothing waits for the count to change: a spin of 1024 pauses first,
a few microseconds, then a park on the count in the kernel; then it tries again.

## Parameters

None.

## Return value

The least element, moved out.

## Complexity

Logarithmic in the size once there is an element, as `try_pop`; the wait is as long as the queue stays empty.

## Exceptions

What the move constructor and the move assignment of `T` throw; none when they are noexcept.

A move of `T` that throws leaves the heap's contents unspecified; the move should not throw.

## Notes

The count of the pushes is read before the attempt, so a push that lands after the attempt found nothing changes
the count and the wait returns at once: no element is missed. A push wakes the parked threads only when some are
counted as waiting. When several threads wait and one element comes, all wake and one takes it; the others wait
again.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    concurrent::priority_queue<int> alarms;

    thread consumer([&alarms] {
        int first = alarms.pop();  // waits until the producer pushes
        println("first alarm {}", first);
    });

    this_thread::sleep_for(10ms);
    alarms.push(42);
    consumer.join();
}
```

Output:

```text
first alarm 42
```

## See also

- [try_pop](try_pop.md): returns at once when the queue is empty
- [push](push.md): inserts an element and wakes a waiting `pop`
- [sgcl::concurrent::priority_queue\<T, Compare\>](README.md)
