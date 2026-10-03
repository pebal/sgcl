[sgcl](../../README.md) › [async](../README.md) › [strand](../strand.md)

# sgcl::async::strand::spawn

```cpp
/*(1)*/ template<class T>
        [[nodiscard]] task<T> spawn(task<T> t);
/*(2)*/ template<class F>
        [[nodiscard]] auto spawn(F f);
```

Starts a task on this strand: it is queued behind the strand's other tasks and run by a worker in its turn, never at
the same time as another task of the strand. The task stays on the strand: whatever wakes it later, it comes back
through the strand's queue. It inherits the [task-locals](../task_local.md) of the task that spawns it.

1. Starts `t`. A task is started once: debug builds assert on a task started already.
2. The same for a coroutine function with captures, passed without the call:
   `s.spawn([x]() -> async::task<int> { ... })`. The closure is moved into a frame of the task's own, so its
   captures live as long as the task. Takes part only when `F` is called with no arguments and returns a task.

The free form, `async::spawn(f(), s)`, is the same ([spawn](../spawn.md)).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to start; not started before |
| `f` | the coroutine function whose task to start |

## Return value

The task, started: `co_await` it or `wait()` for its result. `[[nodiscard]]`: a task object dropped is a task
nobody waits for, which runs to its end all the same; [go](go.md) says that on purpose.

## Complexity

Constant: a push on the strand's queue, with no allocation, and the hand-off to the workers when the strand was idle;
(2) the frame of the task that holds the closure.

## Exceptions

- (1) `std::system_error` when the hand-off to the workers has to start the scheduler and a worker thread cannot be
  started.
- (2) The same, and what the move of `F` throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> add(int& total, int n) {  // on the strand: a plain int, no lock
    for (int i : range(n)) {
        ++total;
        if (i % 100 == 99) {
            co_await async::yield();
        }
    }
}

int main() {
    async::strand serial;
    int total = 0;
    vector<async::task<>> tasks;
    for (int i : range(4)) {
        tasks.push_back(serial.spawn(add(total, 1000)));
    }
    int extra = 5;
    tasks.push_back(serial.spawn([&total, extra]() -> async::task<> {
        total += extra;
        co_return;
    }));
    for (auto& t : tasks) {
        t.wait();
    }
    println("{}", total);
}
```

Output:

```text
4005
```

## See also

- [go](go.md): a task started and let go of
- [executor::spawn](../executor/spawn.md): a task started on an executor
- [spawn](../spawn.md): a task started on the pool of workers
- [sgcl::async::strand](../strand.md)
