[sgcl](../../README.md) › [async](../README.md) › [strand](../strand.md)

# sgcl::async::strand::go

```cpp
template<class T>
void go(task<T> t);    // (1)
template<class F>
void go(F f);          // (2)
```

Starts a task on this strand and lets go of it: a [spawn](spawn.md) and a detach. Nobody waits for the task; it runs
to its end in the strand's turns, and what it returns is dropped. An exception it lets out goes to the handler of
[on_unhandled](../on_unhandled.md).

1. Starts `t`. A task is started once: debug builds assert on a task started already.
2. The same for a coroutine function with captures, passed without the call; the closure is moved into a frame of
   the task's own. Takes part only when `F` is called with no arguments and returns a task.

The free form, `async::go(f(), s)`, is the same ([go](../go.md)).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to start; not started before |
| `f` | the coroutine function whose task to start |

## Return value

None.

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

struct Log {
    vector<string> lines;  // appended to by the strand's tasks only
};

async::task<> write(tracked_ptr<Log> log, string line) {
    log->lines.push_back(line);
    co_return;
}

int main() {
    async::strand serial;
    tracked_ptr log = make_tracked<Log>();
    serial.go(write(log, "opened"));
    serial.go(write(log, "edited"));
    auto last = serial.spawn(write(log, "saved"));  // behind the other two
    last.wait();
    println("{}", log->lines);
}
```

Output:

```text
["opened", "edited", "saved"]
```

## See also

- [spawn](spawn.md): a task started and kept
- [executor::go](../executor/go.md): a task let go of on an executor
- [go](../go.md): a task let go of on the pool of workers
- [sgcl::async::strand](../strand.md)
