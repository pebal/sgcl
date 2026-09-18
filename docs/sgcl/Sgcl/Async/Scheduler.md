# Sgcl::Scheduler, Sgcl::Spawn, Sgcl::Go, Sgcl::Yield

```cpp
#include "sgcl/Sgcl/Async/Task.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    struct Scheduler;                              // the pool of workers, one per process
    template<class T> Task<T> Spawn(Task<T> t);    // the task on the queue of the ready
    template<class T> void Go(Task<T> t);          // spawned and let go of: nobody waits for it
    using Yield = ...;                             // co_await Yield(): to the back of the queue
}
```

The same in the `sgcl` interface: [scheduler, spawn, go, yield](../../async/scheduler.md).

The scheduler runs [tasks](Task.md#task) on a pool of worker threads, one per core (`config::Workers`), each with a queue of its own of the coroutines ready to run and one global queue behind them. A coroutine is made ready by whoever it was waiting for: `Spawn` puts a new task on the queue, a `Send` on a channel makes the receiver that waited ready ([Channel](Channel.md)), a task that ends makes the task that `co_await`ed it ready, `co_await Yield()` puts the running task at the back. Whoever makes a coroutine ready returns at once; a worker runs it, on its own stack, to the coroutine's next suspension. A coroutine that waits is nowhere: a frame on the managed heap and a word on some list, no thread held, so a worker serves as many tasks as are ready and a hundred thousand tasks waiting on a channel cost their frames and nothing else.

The shape is Go's. A worker that makes a coroutine ready puts it on its own queue, and the one it wakes (a receiver served, a task awaited) into its *next* slot, to run as soon as the current coroutine suspends, on the same core, with the cache warm: the rendezvous of two tasks never leaves the worker. A thread that is not a worker puts the coroutine on the global queue. A worker with nothing of its own takes from the global queue, then steals half of another worker's queue; one that finds nothing looks for `config::WorkerSpinMicroseconds` and sleeps. A worker is woken by an enqueue only when none is looking and one sleeps, and a looking worker that finds work wakes the next sleeper, so that there is one looking while work keeps coming and none burning a core when it does not.

What this is, in the terms of Go: `Go` is `go f()`, a task a goroutine, the workers the P's, a channel between tasks the channel of Go; what it is not, yet: goroutines are stackful and preempted, tasks are stackless C++ coroutines (only the body of a coroutine can suspend, a function it calls cannot) and cooperative (a task that computes for a second holds its worker for a second).

The scheduler is started by the first `Spawn` (or the first wake of a waiting coroutine) and stopped when the program ends, or by `Scheduler::Stop()`: its workers are joined, so a task that never suspends never lets the program end, as a thread would not. The queues live in managed objects the scheduler owns; the frames of the queued coroutines are held by the queues' entries and, while they run, by the worker. A worker's queue keeps the words of the entries taken until they are overwritten, so a finished task's frame may stay allocated until its slot is reused (256 slots per worker).

## Rules

- A task on a worker waits with `co_await`: `co_await ch.AsyncReceive()`, `co_await otherTask`, `co_await Yield()`. The blocking calls (`ch.Receive()`, `task.Join()`) park the worker's thread and take it from every other task; debug builds assert on `Join()` from a worker.
- A worker is a thread like any other to the collector: its stack is scanned, the frame it runs is held on it.
- The workers are joined when the scheduler stops: at the end of the program, after `main` returns, or on `Stop()`. `Stop()` is for a program that wants its threads gone at a point of its own, when nothing runs; a task queued at that moment stays queued until the next start. The queues stay across a stop: a task made ready while the workers are being joined (a promise set from a callback, a send from a plain thread) is queued and runs at the next start.
- `config::Workers` (`-DSGCL_WORKERS`, 0 for the hardware concurrency, at most 64) and `config::WorkerSpinMicroseconds` (`-DSGCL_WORKER_SPIN_US`, 20: how long a worker with nothing to run looks for work before it sleeps in the kernel) are compile-time ([config](../../core/config.md)).

## Members

### Scheduler

```cpp
static unsigned Workers();          // the number of workers; starts the scheduler
static bool OnWorker() noexcept;    // whether the calling thread is a worker
static void Stop();                 // the workers joined, the queue let go of; the next Spawn starts it again
static Statistics GetStatistics();  // the queues as they are

struct Statistics {
    unsigned Workers;               // the threads of the pool (0: not started)
    size_t GlobalQueued;            // tasks on the global queue
    size_t LocalQueued;             // tasks on the workers' rings and next slots, together
    unsigned Spinning;              // workers looking for work
    unsigned Sleeping;              // workers asleep in the kernel
};
```

`GetStatistics()` is a look at the load for a benchmark or a monitor: a snapshot, the counts of different words read at different moments.

### Spawn, Go

```cpp
template<class T> Task<T> Spawn(Task<T> t);   // t.Spawn(), and t back
template<class T> void Go(Task<T> t);         // t.Spawn().Detach()
```

`auto t = Spawn(f());` runs `f` on the scheduler and keeps the handle; `Go(f());` runs it and forgets it (a spawn and a detach; `Spawn` is `[[nodiscard]]`: the task object dropped would destroy the coroutine).

### Yield

```cpp
struct Yield {   // an awaitable: co_await Yield()
    bool await_ready() const noexcept;
    template<class P> void await_suspend(std::coroutine_handle<P>);
    void await_resume() const noexcept;
};
```

The running task goes to the back of the queue and the worker takes the next ready one: for a task that has a lot to do and other tasks to be fair to.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A pipeline of tasks: producers send jobs on one channel, workers turn
// each into a result on another, one task sums the results. Nothing
// here is a thread: every wait is a co_await, every task a frame on the
// managed heap, and the scheduler's workers run whichever is ready.
struct Job {
    int id;
};

Task<> Producer(Channel<Ptr<Job>>& jobs, int from, int count) {
    for (int i : Range(count)) {
        co_await jobs.AsyncSend(Make<Job>(from + i));   // suspends while jobs is full
    }
}

Task<> Worker(Channel<Ptr<Job>>& jobs, Channel<int>& results) {
    while (auto job = co_await jobs.AsyncReceive()) {              // suspends while jobs is empty
        co_await results.AsyncSend((*job)->id * 2);
    }
}

Task<long> Summer(Channel<int>& results) {
    long sum = 0;
    while (auto r = co_await results.AsyncReceive()) {
        sum += *r;
    }
    co_return sum;
}

int main() {
    Channel<Ptr<Job>> jobs(8);
    Channel<int> results(8);
    List<Task<>> producers, workers;
    for (int p : Range(4)) {
        producers.Add(Spawn(Producer(jobs, p * 100, 100)));
    }
    for (int w : Range(3)) {
        workers.Add(Spawn(Worker(jobs, results)));
    }
    auto sum = Spawn(Summer(results));
    for (auto& p : producers) {
        p.Join();                                      // this thread waits; the tasks run on the workers
    }
    jobs.Close();                                      // the workers' loops end
    for (auto& w : workers) {
        w.Join();
    }
    results.Close();                                   // the summer's loop ends
    std::cout << sum.Join() << "\n";                   // 2 * (0 + 1 + ... + 399)
    return sum.Result() == 2L * 399 * 400 / 2 ? 0 : 1;
}
```

The output:

```
159600
```

## See also

- [Task](Task.md): the tasks, the frames on the managed heap; [Channel](Channel.md): what tasks wait on
- [README: Coroutines](../../async/README.md#coroutines)
