[sgcl](../README.md) › [async](README.md)

# sgcl::async::yield

```cpp
#include "sgcl/async/scheduler.h"   // or "sgcl/async.h"

namespace sgcl::async {
    // `co_await sgcl::async::yield()`: the task goes to the back of the queue and
    // the worker takes the next ready one
    struct [[nodiscard]] yield {
        bool await_ready() const noexcept;

        template<class P>
        void await_suspend(std::coroutine_handle<P> h);

        void await_resume() const noexcept;
    };
}
```

An awaitable that gives the turn away: `co_await async::yield()` puts the running task at the back of the queue it
runs from, and the thread takes the next ready task; the task goes on from the next line when its turn comes again.
It is for a task that has a lot to do and other tasks to be fair to, since a task is never preempted: one that
computes for a second holds its worker for a second unless it yields. Go's `runtime.Gosched()`, Kotlin's `yield()`,
tokio's `yield_now()`.

On a worker the task goes to the end of the worker's ring, where an idle worker may steal it. On an
[executor](executor/README.md) or a [strand](strand/README.md) it goes to the back of that queue and stays there: a yield on a
strand leaves the strand to the next task, which runs before this one comes back.

## Parameters

None.

## Return value

An awaitable. `co_await async::yield()` always suspends and gives nothing; there is no form for a thread, which
yields with `this_thread::yield()`.

## Complexity

Constant: an enqueue, with no allocation.

## Exceptions

- The construction: none.
- The `co_await`: `std::system_error` when the enqueue has to start the scheduler's workers (a task resumed by hand
  on a plain thread, or a stop in between) and a thread cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> count(string name) {
    for (int i : range(3)) {
        println("{} {}", name, i);
        co_await async::yield();  // the other task runs before the next line
    }
}

int main() {
    async::executor ex;  // one thread, so the order shows
    auto a = ex.spawn(count("a"));
    auto b = ex.spawn(count("b"));
    ex.run_until(a);
    ex.run_until(b);
}
```

Output:

```text
a 0
b 0
a 1
b 1
a 2
b 2
```

## See also

- [scheduler](scheduler/README.md): the queues a task goes back to
- [executor](executor/README.md), [strand](strand/README.md): a yield on them
- [on_workers](on_workers.md), [on](on.md): the other awaitables that move a task
