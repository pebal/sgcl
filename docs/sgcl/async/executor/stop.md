[sgcl](../../README.md) › [async](../README.md) › [executor](../executor.md)

# sgcl::async::executor::stop

```cpp
void stop() noexcept;
```

Makes [run](run.md) return, after the frame it is resuming; a thread parked in `run` is woken to see it. What stops
is the loop, not the tasks: they stay on the queue, suspended, and the next `run()` or [poll](poll.md) resumes them.
A `stop()` with no run in progress makes the next `run()` return at once. It may be called from any thread: a task on
the executor, a signal handler's thread, the program's last line.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> step(async::executor& ex, int n) {
    println("step {}", n);
    if (n == 1) {
        ex.stop();  // the loop ends after this frame; step 2 stays queued
    }
    co_return;
}

int main() {
    async::executor ex;
    for (int n : {1, 2}) {
        ex.go(step(ex, n));
    }
    ex.run();
    println("run returned");
    println("{} left for the next pass", ex.poll());
}
```

Output:

```text
step 1
run returned
step 2
1 left for the next pass
```

## See also

- [run](run.md): the loop a stop ends
- [scheduler::stop](../scheduler/stop.md): the workers joined
- [sgcl::async::executor](../executor.md)
