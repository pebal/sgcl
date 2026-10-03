[sgcl](../../README.md) › [async](../README.md) › [blocking_pool](../blocking_pool.md)

# sgcl::async::blocking_pool::stop

```cpp
static void stop();
```

Runs the jobs queued so far to their end and joins the threads of the pool, for a program that wants them gone at a
point of its own; [scheduler::stop](../scheduler/stop.md) and the end of the program do it too. A job queued while
the threads drain is run before the last one goes. A job that blocks forever holds the stop forever. The next
[spawn_blocking](../spawn_blocking.md) starts the pool again.

## Parameters

None.

## Return value

None.

## Complexity

The jobs still queued, and the join of every thread of the pool.

## Exceptions

`std::system_error` when a thread cannot be joined, as `std::thread::join` reports it.

## Notes

It blocks the calling thread, so it is not called from a task on a worker: debug builds assert.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::spawn_blocking([] { return 1; }).wait();
    println("before the stop: {} threads", async::blocking_pool::get_statistics().threads);
    async::blocking_pool::stop();
    println("after it: {} threads", async::blocking_pool::get_statistics().threads);
    println("{}", async::spawn_blocking([] { return 2; }).wait());  // started again
}
```

Output:

```text
before the stop: 1 threads
after it: 0 threads
2
```

## See also

- [wait_idle](wait_idle.md): the jobs run, the threads kept
- [scheduler::stop](../scheduler/stop.md): the workers, the timers, the reactor and the pool
- [sgcl::async::blocking_pool](../blocking_pool.md)
