[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::executor

```cpp
#include "sgcl/async/executor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class executor;
}
```

`sgcl::async::executor` runs tasks on a thread of the program's choosing. The [scheduler](../scheduler/README.md) runs a task
on whichever worker is free; a platform's UI toolkit (Cocoa, Win32, GTK, the browser) and many a C library demand
one thread, usually the main one, for every call into them. An executor is a queue of frames that only the thread
running the executor resumes. [run](run.md) is that thread's loop: what is queued runs to its next
suspension, an empty queue parks the thread, until [stop](stop.md); `run(t)` is the loop until the task
`t` is done, and its result, which makes a program one task on the main thread:
`int main() { async::executor main; return main.run(program()); }`. [poll](poll.md) is one pass over what
is queued and a return, for a loop that is not the library's (a `CFRunLoop`, a `GMainLoop`, a game's frame), which
calls it when it has a moment.

A task moves to an executor with `co_await async::on(ex)` ([on](../on.md); Kotlin's
`withContext(Dispatchers.Main)`) and back to the pool with `co_await async::on_workers()` ([on_workers](../on_workers.md);
`withContext(Dispatchers.Default)`); [spawn](spawn.md) starts a task on it. A task on an executor stays on
it. Its frame remembers the executor (a word of the header at the front of every managed frame, beside the
[task-locals](../task_local/README.md)), and every wake of the frame, whatever woke it (a channel, a timer, a task it awaited,
a [yield](../yield.md), a select, an event), goes through the scheduler, which routes the frame to the executor's queue.
So a task that sleeps on the main thread is resumed on the main thread though the timer fired on the timer thread,
and a task on the main thread that awaits a channel is resumed on the main thread though a worker served it. A task
that the task awaits (`co_await f()` of a task nobody started) runs where the awaiter runs, as a call would; a task
it starts with [spawn](../spawn.md) or [go](../go.md) runs on the pool, one it starts with the executor's `spawn` or `go`
on the executor.

The executor's thread parks when the queue is empty, on the queue's own word, and a push wakes it, after the spin
every worker does before it sleeps ([set_worker_spin](../scheduler/set_worker_spin.md)), so that a task that hops to the
main thread and back does not pay two trips through the kernel. The queue is a list linked through the frames
themselves, so a push allocates nothing. A [strand](../strand/README.md) is the same queue with no thread of its own.

## Rules

- An executor is run by one thread at a time (`run`, `run_until`, `poll`); debug builds assert on a second. Which
  thread is the program's choice: the main thread for a UI, any thread for a library that wants one.
- [stop](stop.md) stops the loop, not the tasks: `run()` returns after the frame it is resuming, the tasks
  stay queued, suspended, and the next `run()` or `poll()` resumes them; a `stop()` with no run in progress makes the
  next `run()` return at once. From any thread.
- An executor destroyed leaves its tasks suspended for good: the queue is a managed object every frame on it holds
  through its header, so nothing dangles, a wake of such a task lands on a queue nobody runs, and the frames are the
  collector's once nothing else holds them, their destructors never run, as with a task the scheduler was stopped
  under. A program that wants its tasks finished runs the executor until they are.
- [run_until](run_until.md) and `run(t)` wait for the task through a task of the executor: nobody else
  awaits it meanwhile.
- A task on an executor waits with `co_await`, as a task on a worker does: a blocking call there
  (`ch.receive().wait()`, `t.wait()`) blocks the executor's thread and every task queued on it.
- A task on an executor that never suspends holds the executor's thread until it does. Cooperative, as every task is.
- The executor holds its queue through a root: it lives anywhere (a local of `main`, a global, a member).
  Neither copyable nor movable.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](executor.md) | constructs an executor with an empty queue |
| `(destructor)` | leaves the tasks still queued suspended for good |

#### Observers

| Function | Description |
|---|---|
| [running](running.md) | checks whether a `run` or a `poll` is in progress |

#### Operations

| Function | Description |
|---|---|
| [run](run.md) | the calling thread's loop, until a stop or the end of a task |
| [run_until](run_until.md) | the loop until a task is done |
| [poll](poll.md) | one pass over what is queued |
| [stop](stop.md) | makes `run` return; the tasks stay queued |

#### Starting tasks

| Function | Description |
|---|---|
| [spawn](spawn.md) | starts a task on the executor |
| [go](go.md) | starts a task on the executor and lets go of it |

## Complexity

A push on the queue (a spawn, a wake, an `on`) is an exchange of the tail and a store of a link, with no
allocation, and a wake of the parked thread when it sleeps. A resume is a call.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

// A program that is one task on the main thread, the way a program with a UI
// is: the widgets on the main thread only, the work on the pool, and a strand
// for the tasks that share a counter.
thread::id main_thread;

bool on_main() {
    return this_thread::get_id() == main_thread;
}

async::task<long> compute(int n) {  // an awaited task runs where its awaiter runs
    long sum = 0;
    for (int i : range(n)) {
        sum += i;
    }
    co_return sum;
}

async::task<> increment(int& counter, int n) {  // on a strand: a plain int, no lock
    for (int i : range(n)) {
        ++counter;
        if (i % 100 == 99) {
            co_await async::yield();  // the strand to the next task
        }
    }
}

async::task<int> program(async::executor& main, async::strand& serial) {
    println("starts on the main thread: {}", on_main());
    co_await async::on_workers();
    println("computes on a worker: {}", async::scheduler::on_worker());
    long sum = co_await compute(1000);
    co_await async::on(main);
    println("back on the main thread: {}, the sum {}", on_main(), sum);
    co_await async::sleep(1ms);  // the timer thread wakes it, on the main thread
    println("after a sleep, on the main thread: {}", on_main());
    int counter = 0;
    vector<async::task<>> tasks;
    for (int i : range(3)) {
        tasks.push_back(serial.spawn(increment(counter, 1000)));
    }
    for (auto& t : tasks) {
        co_await t;
    }
    println("three tasks on a strand: {}, on the main thread: {}", counter, on_main());
    co_return 0;
}

int main() {
    main_thread = this_thread::get_id();
    async::executor main;
    async::strand serial;
    return main.run(program(main, serial));
}
```

Output:

```text
starts on the main thread: true
computes on a worker: true
back on the main thread: true, the sum 499500
after a sleep, on the main thread: true
three tasks on a strand: 3000, on the main thread: true
```

## See also

- [strand](../strand/README.md): an executor with no thread of its own
- [on](../on.md), [on_workers](../on_workers.md): a task moved to an executor and back
- [scheduler](../scheduler/README.md): the pool of workers
- [task_local](../task_local/README.md): a value a task and its children see, kept in the same header of the frame
- [mutex](../mutex/README.md): what holds across a wait
- [README: Coroutines](../README.md#coroutines)
