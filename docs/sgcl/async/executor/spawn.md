[sgcl](../../README.md) › [async](../README.md) › [executor](README.md)

# sgcl::async::executor::spawn

```cpp
template<class T>
[[nodiscard]] task<T> spawn(task<T> t);    // (1)
template<class F>
[[nodiscard]] auto spawn(F f);             // (2)
```

Starts a task on this executor: it is queued here, and runs when the thread that runs the executor comes to it. The
task stays on the executor: whatever wakes it later, it is resumed on the executor's thread. It inherits the
[task-locals](../task_local/README.md) of the task that spawns it.

1. Starts `t`. A task is started once: debug builds assert on a task started already.
2. The same for a coroutine function with captures, passed without the call:
   `ex.spawn([x]() -> async::task<int> { ... })`. The closure is moved into a frame of the task's own, so its
   captures live as long as the task. Takes part only when `F` is called with no arguments and returns a task.

The free form, `async::spawn(f(), ex)`, is the same ([spawn](../spawn.md)).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to start; not started before |
| `f` | the coroutine function whose task to start |

## Return value

The task, started: `co_await` it or `wait()` for its result. `[[nodiscard]]`: a task object dropped is a task
nobody waits for, which runs to its end all the same; [go](go.md) says that on purpose.

## Complexity

Constant: a push on the executor's queue, with no allocation; (2) the frame of the task that holds the closure.

## Exceptions

- (1) None.
- (2) What the move of `F` throws.

## Notes

The task of (2) is a task of the library's that holds the closure and awaits the function's task; the function's
task starts when that task first runs, so on an executor it runs one turn later than a task passed to (1).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> twice(int x) {
    co_return 2 * x;
}

int main() {
    async::executor ex;
    auto a = ex.spawn(twice(10));
    int base = 5;
    auto b = ex.spawn([base]() -> async::task<int> {  // the closure kept with the task
        co_return base + 1;
    });
    ex.run_until(a);
    ex.run_until(b);
    println("{} {}", a.result(), b.result());
}
```

Output:

```text
20 6
```

## See also

- [go](go.md): a task started and let go of
- [spawn](../spawn.md): the same on the pool of workers, and `spawn(t, ex)`
- [strand::spawn](../strand/spawn.md): a task started on a strand
- [sgcl::async::executor](README.md)
