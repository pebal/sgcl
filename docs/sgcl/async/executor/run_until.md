[sgcl](../../README.md) › [async](../README.md) › [executor](../executor.md)

# sgcl::async::executor::run_until

```cpp
template<class T>
void run_until(task<T>& t) noexcept;
```

Runs the executor on the calling thread until `t` is done, or until [stop](stop.md). A task nobody has started is
started on this executor. The executor waits for the end of `t` through a task of its own that awaits it, so the end
of `t` is a wake of this loop, whatever thread `t` ended on; the result stays in `t`, for `t.result()`. A task that
is done already returns at once.

A stop before `t` ends returns with `t` still running; the next `run_until(t)` of the same task takes the wait up
again.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to run the loop for; nobody else may await it meanwhile |

## Return value

None.

## Complexity

The loop runs until `t` ends: each turn takes a frame off the queue (constant) and resumes it.

## Exceptions

None: what `t` throws stays in it, for `t.result()`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> square(int x) {
    co_await async::yield();
    co_return x * x;
}

int main() {
    async::executor ex;
    auto t = ex.spawn(square(7));
    ex.run_until(t);
    println("done: {}, the result {}", t.done(), t.result());
}
```

Output:

```text
done: true, the result 49
```

## See also

- [run](run.md): the loop until a stop, or until a task is done and its result
- [poll](poll.md): one pass
- [sgcl::async::executor](../executor.md)
