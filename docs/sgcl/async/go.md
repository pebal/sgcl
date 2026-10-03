[sgcl](../README.md) › [async](README.md)

# sgcl::async::go

```cpp
#include "sgcl/async/coroutine.h"   // or "sgcl/async.h"
#include "sgcl/async/executor.h"    // (3–4), or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    void go(task<T> t);                  // (1)
    template<class F>
    void go(F f);                        // (2)
    template<class T, class Executor>
    void go(task<T> t, Executor& ex);    // (3)
    template<class F, class Executor>
    void go(F f, Executor& ex);          // (4)
}
```

Starts a task and lets go of it, Go's `go f()`: for a task nobody waits for. The task runs on to its end, and
destroys its frame itself then; the memory is the collector's from then on. It is [spawn](spawn.md) and
[detach](task/detach.md) in one, `t.spawn().detach()`, under the name that says no handle is kept.

1. Puts the task on the scheduler's queue of the ready and lets go of it.
2. The same for the task of the coroutine function `f`, passed uncalled: a lambda with captures,
   `async::go([&ch]() -> async::task<> { ... })`, Go's `go func() { ... }()`. The closure is copied into the frame of
   a task of its own, which lives as long as the task, and the captures with it ([spawn](spawn.md) says why a called
   lambda with captures would not do). Takes part only when `f()` returns a task.
3. Starts the task on `ex`, an [executor](executor.md) or a [strand](strand.md), and lets go of it; `ex.go(t)`.
4. (2) on `ex`.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to start: one nobody started yet |
| `f` | the coroutine function, called once with no arguments, inside the task |
| `ex` | the executor or the strand to start the task on |

## Return value

None.

## Complexity

- (1) Constant: a push on a queue of the scheduler, and the wake of a sleeping worker when none is looking for work.
- (2) The same, plus the frame of the task that holds the closure.
- (3), (4) A push on the queue of `ex`, plus the frame of (4).

## Exceptions

- (1)–(2) `std::system_error` when the push starts the scheduler (the first start, or the first after a stop) and a
  worker's thread cannot be started; (2) also what the move constructor of `F` throws.
- (3)–(4) What the `go` of `ex` throws ([executor](executor.md), [strand](strand.md)).

What the task throws, nobody reads: it goes to [on_unhandled](on_unhandled.md)'s handler, which by default prints it
and ends the program, as a goroutine's panic ends a Go program.

## Notes

Letting go is not cancelling: a task started by `go` that is to stop early is given a
[stop_token](stop_token.md) and looks at it. A task that nothing will wake again lives as long as what it waits for,
and is collected with it ([detach](task/detach.md)).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> worker(async::channel<int> jobs, async::channel<int> results) {
    while (auto job = co_await jobs.receive()) {
        co_await results.send(*job * *job);
    }
}

int main() {
    async::channel<int> jobs(8), results(8);
    for (int w : range(4)) {
        async::go(worker(jobs, results));  // nobody waits for them; they end when jobs is closed
    }
    async::go([jobs]() -> async::task<> {  // a lambda with captures, passed uncalled
        for (int i : range(1, 11)) {
            co_await jobs.send(i);
        }
        jobs.close();
    });
    int sum = 0;
    for (int i : range(10)) {
        sum += *results.receive().wait();
    }
    println("{}", sum);
}
```

Output:

```text
385
```

## See also

- [spawn](spawn.md): a start that keeps the handle, for the result
- [go_blocking](go_blocking.md): a blocking call on the blocking pool, let go of the same way
- [detach](task/detach.md): lets go of a task already started
- [on_unhandled](on_unhandled.md): what becomes of what the task throws
- [task_group](task_group.md): tasks started together, waited for and stopped as one
