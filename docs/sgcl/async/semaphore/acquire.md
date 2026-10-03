[sgcl](../../README.md) › [async](../README.md) › [semaphore](../semaphore.md)

# sgcl::async::semaphore::acquire

```cpp
auto acquire() noexcept;
```

Takes a permit, waiting while there is none: a receive of one of the channel's signals. The call does nothing yet;
it returns an [operation](../operation.md), carried out in one of two ways
([README: Waiting operations](../README.md#waiting-operations)):

- `co_await s.acquire()` in a task: the task suspends while no permit is free, holding no thread, and is resumed by
  the [release](release.md) that gives it one.
- `s.acquire().wait()` on a thread: blocks the thread until a permit is free.

## Parameters

None.

## Return value

An [operation](../operation.md). Carried out, by `co_await` or by `.wait()`, it gives a `bool`: `true`, a permit
taken. The channel of a semaphore is never closed, so the wait ends only with a permit.

## Complexity

Constant when a permit is free: a receive through the channel's ring. Otherwise a waiter is registered on the
channel's list until a release serves it.

## Exceptions

The call: none. Carried out: the receive is not `noexcept`, as a receive of a channel is not: it may wake a waiting
sender, and the wake may start the scheduler's workers (`std::system_error` when a thread cannot be started). A
semaphore has no waiting sender, since [release](release.md) never waits.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> worker(async::semaphore& start) {
    co_await start.acquire();  // waits for main's release
    println("the task got a permit");
}

int main() {
    async::semaphore start(0);
    async::task<> t = async::spawn(worker(start));
    println("main releases");
    start.release();
    t.wait();

    start.release();
    println("{}", start.acquire().wait());  // a thread
}
```

Output:

```text
main releases
the task got a permit
true
```

## See also

- [try_acquire](try_acquire.md): a permit without the wait
- [release](release.md): gives a permit back
- [on_acquire](on_acquire.md): the acquire as a case of a select
- [sgcl::async::semaphore](../semaphore.md)
