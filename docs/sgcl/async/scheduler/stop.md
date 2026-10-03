[sgcl](../../README.md) › [async](../README.md) › [scheduler](../scheduler.md)

# sgcl::async::scheduler::stop

```cpp
static void stop();
```

Joins the workers, for a program that wants its threads gone at a point of its own; the end of the program does
the same. The timer thread, the [reactor](../readable.md) and the [blocking pool](../blocking_pool.md) are stopped
first, in that order, those of them that were started: the waits still registered with the reactor end with
nothing (their events set), and the jobs queued on the pool run to their end. The workers then run what is ready
and leave; a task that never suspends holds the stop, as it would hold the end of the program.

The queues stay. A frame made ready while the workers are being joined (a promise set from a callback, a send from a
plain thread) is queued, and runs at the next start; a task suspended at the stop stays suspended until something
makes it ready. The next enqueue, a [spawn](../spawn.md) or a wake, starts the scheduler again, and the next wait
on a timer, on the reactor or on the pool starts that again.

## Parameters

None.

## Return value

None.

## Complexity

The join of every worker and of the threads of the timers, the reactor and the pool.

## Exceptions

`std::system_error` when a thread cannot be joined, as `std::thread::join` reports it.

## Notes

The call joins the workers, so it is never made from a task: debug builds assert. A program stops the scheduler
when nothing runs.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> answer() {
    co_return 42;
}

int main() {
    async::scheduler::set_workers(2);
    println("{}", async::spawn(answer()).wait());
    async::scheduler::stop();
    println("after the stop: {} workers", async::scheduler::get_statistics().workers);

    println("{}", async::spawn(answer()).wait());  // started again
    println("after the spawn: {} workers", async::scheduler::get_statistics().workers);
}
```

Output:

```text
42
after the stop: 0 workers
42
after the spawn: 2 workers
```

## See also

- [set_workers](set_workers.md): stops and starts the workers with another number
- [blocking_pool::stop](../blocking_pool/stop.md): the blocking pool alone
- [sgcl::async::scheduler](../scheduler.md)
