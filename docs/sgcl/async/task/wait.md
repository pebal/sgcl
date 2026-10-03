[sgcl](../../README.md) › [async](../README.md) › [task](../task.md)

# sgcl::async::task\<T\>::wait, operator co_await

```cpp
/*(1)*/ T& wait();
/*(2)*/ awaiter operator co_await() noexcept;
/*(3)*/ void wait();
/*(4)*/ awaiter operator co_await() noexcept;
```

Waits until the task is done and gives its value, or rethrows what the coroutine threw: `t.wait()` on a thread,
`co_await t` in a task. A task nobody started yet is started by the wait, so `f().wait()` and `co_await f()` run a
task that was never spawned.

1. Blocks the calling thread until the task is done, then returns [result](result.md), as
   `std::this_thread::sync_wait` gives a sender's result. A task nobody started is put on the scheduler first:
   `f().wait()` is `async::spawn(f()).wait()`. For a thread; a task on a worker `co_await`s instead, since the wait
   would hold the worker and every task it would run (debug builds assert it).
2. `co_await t`: suspends the awaiting coroutine until the task is done, with no thread held, and gives the value,
   moved out of the frame. A task that is done already does not suspend. A task nobody started is started where the
   awaiting task runs (its executor or its strand, the pool otherwise), as a call would run it, so that what the
   awaiter is guaranteed holds for what it awaits.
3. `task<void>`: as (1), with no value.
4. `task<void>`: as (2), with no value.

## Parameters

None.

## Return value

- (1) A reference to the value in the frame, valid while the task object holds the frame.
- (2) The awaiter of `co_await t`, which gives a `T` moved out of the frame.
- (3) None.
- (4) The awaiter of `co_await t`, which gives nothing.

## Complexity

- (1), (3) The time the task takes to end; one load for a task that is done.
- (2), (4) Nothing for a task that is done. Otherwise one compare-exchange installs the awaiting coroutine, and the
  end of the task hands it to the scheduler, to run next on the worker that ended the task.

## Exceptions

- (1), (3) What the coroutine threw, rethrown; `std::system_error` when the wait starts the scheduler and a worker's
  thread cannot be started.
- (2), (4) The call throws nothing; carried out, `co_await t` rethrows what the coroutine threw, and throws
  `std::system_error`, in the awaiting task, when `t` was not started yet and its start has to start the
  scheduler and a worker's thread cannot be started (`t`, queued, runs when the workers next start).

## Notes

A task is awaited by one coroutine at a time (debug builds assert it); the same coroutine may await it again after,
and a second `co_await` of a `task<T>` gives what the first one's move left in the frame. A coroutine that awaits a
task needs a managed frame: a task, or a coroutine whose promise derives from
[managed_frame](../../core/managed_frame.md).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

async::task<int> square(int x) {
    co_return x * x;
}

async::task<int> sum_of_squares(int n) {
    int s = 0;
    for (int i : range(1, n + 1)) {
        s += co_await square(i);  // started where this task runs, waited for without a thread
    }
    co_return s;
}

async::task<> fails() {
    throw std::runtime_error("broken");
    co_return;
}

int main() {
    println("{}", sum_of_squares(3).wait());  // put on the scheduler, waited for on this thread
    try {
        fails().wait();
    } catch (const std::exception& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
14
broken
```

## See also

- [result](result.md): the value of a task that is done
- [done](done.md): checks whether the task has ended, without waiting
- [when_all](../when_all.md), [when_any](../when_any.md): a wait for several tasks
- [run](../run.md): the wait of `main` for the program's task
- [sgcl::async::task\<T\>](../task.md)
