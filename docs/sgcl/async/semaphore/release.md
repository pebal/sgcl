[sgcl](../../README.md) › [async](../README.md) › [semaphore](README.md)

# sgcl::async::semaphore::release

```cpp
void release();
```

Gives a permit back: a send of a signal into the channel, which hands it to the first waiter when there is one, a
thread or a task in [acquire](acquire.md) or a select with an [on_acquire](on_acquire.md) case. Any thread or task
may release, not only one that acquired: a semaphore counts permits, it does not know their holders.

A release that finds the semaphore at its maximum is lost: the semaphore never holds more permits than the maximum
its constructor gave it.

## Parameters

None.

## Return value

None.

## Complexity

Constant: a send into the ring, and a wake when a waiter is there.

## Exceptions

`std::system_error` when the wake of a waiting task must start the scheduler's workers and a thread cannot be
started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::semaphore slots(1, 2);
    slots.release();
    println("{}", slots.available());
    slots.release();  // at the maximum: lost
    println("{}", slots.available());
}
```

Output:

```text
2
2
```

## See also

- [acquire](acquire.md), [try_acquire](try_acquire.md), [on_acquire](on_acquire.md): take a permit
- [sgcl::async::semaphore](README.md)
