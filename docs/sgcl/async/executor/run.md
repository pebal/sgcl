[sgcl](../../README.md) › [async](../README.md) › [executor](README.md)

# sgcl::async::executor::run

```cpp
void run() noexcept;    // (1)
template<class T>
T run(task<T> t);       // (2)
void run(task<> t);     // (3)
```

Runs the executor on the calling thread: the frames queued run, each to its next suspension; an empty queue parks
the thread, after a spin, until a push wakes it.

1. The loop until [stop](stop.md). A `stop()` made before the call, with no run in progress, makes it return at once.
2. The loop until `t` is done, and its result: the main function's idiom, `return main.run(program());`. A task
   nobody has started is started on this executor; one started elsewhere is waited for where it runs, its end a
   wake of this loop ([run_until](run_until.md)).
3. The same for a task of nothing.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to run the loop for |

## Return value

- (1) None.
- (2) What `t` returned, moved out of it.
- (3) None.

## Complexity

The loop runs as long as the program wants it to: each turn takes a frame off the queue (constant) and resumes it.

## Exceptions

- (1) None: what a task throws stays in the task, for whoever waits for it.
- (2–3) What `t` threw, rethrown.

A [stop](stop.md) before `t` ends leaves (2) and (3) with nothing to return: debug builds assert.

## Notes

The executor is run by one thread at a time: a second `run` or a [poll](poll.md) at once is asserted on in debug
builds. A blocking wait in a task on the executor blocks the loop; tasks on it wait with `co_await`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> program() {
    println("the program runs on the main thread");
    co_return 3;
}

async::task<> last(async::executor& ex) {
    println("the last task stops the loop");
    ex.stop();
    co_return;
}

int main() {
    async::executor main;
    int code = main.run(program());
    println("run(t) gave {}", code);

    main.go(last(main));
    main.run();
    println("run() returned");
}
```

Output:

```text
the program runs on the main thread
run(t) gave 3
the last task stops the loop
run() returned
```

## See also

- [run_until](run_until.md): the loop until a task is done, its result left in it
- [poll](poll.md): one pass, for a loop of the program's own
- [stop](stop.md): makes `run` return
- [sgcl::async::executor](README.md)
