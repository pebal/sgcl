[sgcl](../README.md) › [async](README.md)

# sgcl::async::scheduler

```cpp
#include "sgcl/async/scheduler.h"   // or "sgcl/async.h"

namespace sgcl::async {
    struct scheduler;
}
```

`sgcl::async::scheduler` is the program's view of the pool that runs the [tasks](task.md): worker threads, one per
core, each with a queue of its own of the coroutines ready to run, and one global queue behind them. There is one
scheduler per process, and every member is static. A coroutine is made ready by whoever it was waiting for:
[spawn](spawn.md) puts a new task on a queue, a send on a [channel](channel.md) makes the receiver that waited
ready, a task that ends makes the task that awaited it ready, [yield](yield.md) puts the running task at the back.
Whoever makes a coroutine ready returns at once; a worker runs it, on its own stack, to the coroutine's next
suspension. A coroutine that waits is nowhere: a frame on the managed heap and a word on some list, no thread held,
so a worker serves as many tasks as are ready, and a hundred thousand tasks waiting on a channel cost their frames
and nothing else.

The shape is Go's. A worker that makes a coroutine ready puts it on its own queue, and the one it wakes (a receiver
served, a task awaited) into its *next* slot, to run as soon as the current coroutine suspends, on the same core,
with the cache warm: the rendezvous of two tasks never leaves the worker. A thread that is not a worker puts the
coroutine on the global queue. A worker with nothing of its own takes from the global queue, then steals half of
another worker's queue; one that finds nothing looks for the spin time ([set_worker_spin](scheduler/set_worker_spin.md))
and sleeps in the kernel. A worker is woken by an enqueue only when none is looking and one sleeps, and a looking
worker that finds work wakes the next sleeper, so that one looks while work keeps coming and none burns a core when
it does not.

In the terms of Go: `spawn` is `go f()`, a task a goroutine, the workers the P's (`GOMAXPROCS` is
[set_workers](scheduler/set_workers.md), `SGCL_WORKERS` in the environment), a channel between tasks the channel of
Go. What it is not: goroutines are stackful and preempted, tasks are stackless C++ coroutines (only the body of a
coroutine can suspend, a function it calls cannot) and cooperative (a task that computes for a second holds its
worker for a second).

## Rules

- A task on a worker waits with `co_await`: `co_await ch.receive()`, `co_await other_task`,
  `co_await async::yield()`. The blocking forms (`ch.receive().wait()`, `t.wait()`) park the worker's thread and take
  it from every other task; debug builds assert on a task's `wait()` from a worker
  ([Waiting operations](README.md#waiting-operations)).
- A worker is a thread like any other to the collector: its stack is scanned, the frame it runs is held on it.
  Before it looks for work a worker clears the dead part of its stack (4 KB in a build with `NDEBUG`, 64 KB
  without; `SGCL_WORKER_STACK_CLEAR` sets it), where the frames of the task that just ran left words the
  collector's conservative scan would take for roots while the worker idles.
- The scheduler is started by the first enqueue (a [spawn](spawn.md), the first wake of a waiting coroutine) and by
  [workers](scheduler/workers.md), and stopped when the program ends or by [stop](scheduler/stop.md); the workers are
  joined then, so a task that never suspends never lets the program end, as a thread would not. The next enqueue
  starts it again. A start whose worker thread the system refuses throws `std::system_error` and leaves the
  scheduler stopped: the workers made before it are joined without having run anything, and the frame whose
  enqueue started it is on the global queue already, for the next start.
- The queues live in managed objects the scheduler owns, and stay across a stop: a frame made ready while the
  workers are being joined (a promise set from a callback, a send from a plain thread) is queued and runs at the
  next start. A frame on a queue is held by the queue's word, and while it runs by the worker; a slot a frame was
  taken from is cleared, so a ring holds no frame but the ones queued on it.
- The number of workers is the program's ([set_workers](scheduler/set_workers.md)), else the environment's
  (`SGCL_WORKERS`, read once when the scheduler first starts, as Go reads `GOMAXPROCS`), else the build's
  (`config::workers`, `-DSGCL_WORKERS`); 0 in any of them is the hardware concurrency, and there are at most 64.
- The spin, how long a worker with nothing to run looks for work before it sleeps, is the program's
  ([set_worker_spin](scheduler/set_worker_spin.md)), else `SGCL_WORKER_SPIN_US`, else
  `config::worker_spin_microseconds` (20 µs). A value of the environment that does not read (`SGCL_WORKERS=abc`) is
  ignored with one line on stderr, and the default taken; nothing of these settings is read on a task's path.
- [stop](scheduler/stop.md) and [set_workers](scheduler/set_workers.md) join the workers: never from a task (debug
  builds assert). A program changes them at a quiet moment.
- A task is cooperative: one that never suspends holds its worker until it does. A worker takes from the global
  queue once in 61 turns even while its own ring has work, so that the tasks there are not starved by a busy ring.

## Member types

| Type | Definition |
|---|---|
| [statistics](scheduler-statistics.md) | the queues and the workers as they are, what `get_statistics` returns |

## Member functions

#### Observers

| Function | Description |
|---|---|
| [workers](scheduler/workers.md) | the number of workers; starts the scheduler (static) |
| [worker_spin](scheduler/worker_spin.md) | how long a worker looks for work before it sleeps (static) |
| [on_worker](scheduler/on_worker.md) | checks whether the calling thread is a worker (static) |
| [get_statistics](scheduler/get_statistics.md) | the queues and the workers as they are (static) |

#### Modifiers

| Function | Description |
|---|---|
| [set_workers](scheduler/set_workers.md) | sets the number of workers (static) |
| [set_worker_spin](scheduler/set_worker_spin.md) | sets how long a worker looks for work before it sleeps (static) |

#### Operations

| Function | Description |
|---|---|
| [stop](scheduler/stop.md) | joins the workers, and stops the timers, the reactor and the blocking pool (static) |

## Complexity

Making a coroutine ready is constant: on a worker a store into its ring (a ring of 256; a full one spills to the
global queue) or into its next slot, from another thread a push on the global queue, and a wake through the kernel
only when no worker is looking and one sleeps. A resume is a call. A coroutine that waits costs its frame and
nothing else.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// Producers send jobs on one channel, workers turn each into a result on
// another, one task sums the results. Nothing here is a thread: every wait
// is a co_await, and the scheduler's workers run whichever task is ready.
struct Job {
    int id;
};

async::task<> producer(async::channel<tracked_ptr<Job>> jobs, int from, int count) {
    for (int i : range(count)) {
        co_await jobs.send(make_tracked<Job>(from + i));  // suspends while jobs is full
    }
}

async::task<> worker(async::channel<tracked_ptr<Job>> jobs, async::channel<int> results) {
    while (auto job = co_await jobs.receive()) {  // suspends while jobs is empty
        co_await results.send((*job)->id * 2);
    }
}

async::task<long> summer(async::channel<int> results) {
    long sum = 0;
    while (auto r = co_await results.receive()) {
        sum += *r;
    }
    co_return sum;
}

int main() {
    async::channel<tracked_ptr<Job>> jobs(8);
    async::channel<int> results(8);
    vector<async::task<>> producers, workers;
    for (int p : range(4)) {
        producers.push_back(async::spawn(producer(jobs, p * 100, 100)));
    }
    for (int w : range(3)) {
        workers.push_back(async::spawn(worker(jobs, results)));
    }
    auto sum = async::spawn(summer(results));
    for (auto& p : producers) {
        p.wait();  // this thread waits; the tasks run on the workers
    }
    jobs.close();  // the workers' loops end
    for (auto& w : workers) {
        w.wait();
    }
    results.close();  // the summer's loop ends
    println("{}", sum.wait());
}
```

Output:

```text
159600
```

## See also

- [task](task.md), [spawn](spawn.md), [go](go.md): what the scheduler runs, and how it gets there
- [yield](yield.md): to the back of the queue
- [executor](executor.md), [strand](strand.md): a task on a thread of the program's choosing, or one at a time
- [spawn_blocking](spawn_blocking.md): a blocking call off the workers
- [coroutine](../core/coroutine.md): the frames on the managed heap
- [README: Coroutines](README.md#coroutines)
