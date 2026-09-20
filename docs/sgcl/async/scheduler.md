# sgcl::scheduler, sgcl::spawn, sgcl::yield

```cpp
#include "sgcl/async/scheduler.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    struct scheduler;                              // the pool of workers, one per process
    template<class T> task<T> spawn(task<T> t);    // the task on the queue of the ready
    template<class T> void go(task<T> t);          // spawned and let go of: nobody waits for it
    template<class F> auto spawn(F f);             // the same for a coroutine function with captures: spawn([x]() -> task<int> { ... })
    template<class F> void go(F f);
    struct yield;                                  // co_await yield(): to the back of the queue
}
```

The scheduler runs [tasks](coroutine.md#task) on a pool of worker threads, one per core (`config::Workers`), each with a queue of its own of the coroutines ready to run and one global queue behind them. A coroutine is made ready by whoever it was waiting for: `spawn` puts a new task on the queue, a `send` on a channel makes the receiver that waited ready ([channel](channel.md)), a task that ends makes the task that `co_await`ed it ready, `co_await yield()` puts the running task at the back. Whoever makes a coroutine ready returns at once; a worker runs it, on its own stack, to the coroutine's next suspension. A coroutine that waits is nowhere: a frame on the managed heap and a word on some list, no thread held, so a worker serves as many tasks as are ready and a hundred thousand tasks waiting on a channel cost their frames and nothing else.

The shape is Go's. A worker that makes a coroutine ready puts it on its own queue, and the one it wakes (a receiver served, a task awaited) into its *next* slot, to run as soon as the current coroutine suspends, on the same core, with the cache warm: the rendezvous of two tasks never leaves the worker. A thread that is not a worker puts the coroutine on the global queue. A worker with nothing of its own clears the dead part of its stack first (a page: the frames of the task that just ran, whose words the collector's conservative scan would still take for roots while the worker idles, so an object referenced only there lived on), then takes from the global queue, then steals half of another worker's queue; one that finds nothing looks for `config::WorkerSpinMicroseconds` and sleeps. A worker is woken by an enqueue only when none is looking and one sleeps, and a looking worker that finds work wakes the next sleeper, so that there is one looking while work keeps coming and none burning a core when it does not.

What this is, in the terms of Go: `spawn` is `go f()`, a task a goroutine, the workers the P's, a channel between tasks the channel of Go; what it is not, yet: goroutines are stackful and preempted, tasks are stackless C++ coroutines (only the body of a coroutine can suspend, a function it calls cannot) and cooperative (a task that computes for a second holds its worker for a second).

The scheduler is started by the first `spawn` (or the first wake of a waiting coroutine) and stopped when the program ends, or by `scheduler::stop()`: its workers are joined, so a task that never suspends never lets the program end, as a thread would not. The queues live in managed objects the scheduler owns; the frames of the queued coroutines are held by the queues' entries and, while they run, by the worker. A worker's queue keeps the words of the entries taken until they are overwritten, so a finished task's frame may stay allocated until its slot is reused (256 slots per worker).

## Rules

- A task on a worker waits with `co_await`: `co_await ch.async_receive()`, `co_await other_task`, `co_await yield()`. The blocking calls (`ch.receive()`, `task.join()`) park the worker's thread and take it from every other task; debug builds assert on `join()` from a worker.
- A worker is a thread like any other to the collector: its stack is scanned, the frame it runs is held on it.
- The workers are joined when the scheduler stops: at the end of the program, after `main` returns, or on `stop()`. `stop()` is for a program that wants its threads gone at a point of its own, when nothing runs; a task queued at that moment stays queued until the next start. The queues stay across a stop: a task made ready while the workers are being joined (a promise set from a callback, a send from a plain thread) is queued and runs at the next start.
- `config::Workers` (`-DSGCL_WORKERS`, 0 for the hardware concurrency, at most 64) and `config::WorkerSpinMicroseconds` (`-DSGCL_WORKER_SPIN_US`, 20: how long a worker with nothing to run looks for work before it sleeps in the kernel) are compile-time ([config](../core/config.md)).

## Members

### scheduler

```cpp
static unsigned workers();          // the number of workers; starts the scheduler
static bool on_worker() noexcept;   // whether the calling thread is a worker
static void stop();                 // the workers joined, the queue let go of; the next spawn starts it again
static statistics get_statistics(); // the queues as they are

struct statistics {
    unsigned workers;               // the threads of the pool (0: not started)
    size_t global_queued;           // tasks on the global queue
    size_t local_queued;            // tasks on the workers' rings and next slots, together
    unsigned spinning;              // workers looking for work
    unsigned sleeping;              // workers asleep in the kernel
};
```

`get_statistics()` is a look at the load for a benchmark or a monitor: a snapshot, the counts of different words read at different moments.

### spawn

```cpp
template<class T> task<T> spawn(task<T> t);   // t.spawn(), and t back
template<class T> void go(task<T> t);         // t.spawn().detach()
```

`auto t = sgcl::spawn(f());` runs `f` on the scheduler and keeps the handle; `sgcl::go(f());` runs it and forgets it (a spawn and a detach; `spawn` is `[[nodiscard]]`: the task object dropped would destroy the coroutine).

```cpp
template<class F> auto spawn(F f);   // F: a callable returning a task; the closure copied into a frame of the task's own
template<class F> void go(F f);
```

The same given the coroutine function rather than its task — a lambda **with captures**, passed without the call: `sgcl::spawn([x]() -> task<int> { ... })`, `sgcl::go([&ch]() -> task<> { ... })`. A lambda's captures are fields of the closure object, and a coroutine's frame keeps the closure by `this`, not by copy (only the parameters of a coroutine are copied into its frame), so the task of a called lambda, `spawn([x]() -> task<int> { ... }())`, runs on a closure that died at the end of that statement and reads freed stack (CppCoreGuidelines CP.51; AddressSanitizer reports a stack-use-after-scope). Passed uncalled, the closure is copied into a frame that lives as long as the task, and the captures with it — a `tracked_ptr` captured is a root of the task, as a local would be. A lambda without captures, or a named coroutine with parameters, may be called and its task passed; one with captures is passed itself. The forms of Go: `go f()` with `spawn(f())`, `go func() { ... }()` with `go([&]() -> task<> { ... })`.

```cpp
int n = 7;
tracked_ptr node = make_tracked<Node>();
auto t = spawn([n, node]() -> task<int> {   // the closure lives in the task's frame; node is rooted by it
    co_await sgcl::sleep(10ms);
    co_return node->value + n;
});
```

### yield

```cpp
struct yield {   // an awaitable: co_await yield()
    bool await_ready() const noexcept;
    template<class P> void await_suspend(std::coroutine_handle<P>);
    void await_resume() const noexcept;
};
```

The running task goes to the back of the queue and the worker takes the next ready one: for a task that has a lot to do and other tasks to be fair to.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A pipeline of tasks: producers send jobs on one channel, workers turn
// each into a result on another, one task sums the results. Nothing
// here is a thread: every wait is a co_await, every task a frame on the
// managed heap, and the scheduler's workers run whichever is ready.
struct Job {
    int id;
};

task<> producer(channel<tracked_ptr<Job>>& jobs, int from, int count) {
    for (int i : range(count)) {
        co_await jobs.async_send(make_tracked<Job>(from + i));   // suspends while jobs is full
    }
}

task<> worker(channel<tracked_ptr<Job>>& jobs, channel<int>& results) {
    while (auto job = co_await jobs.async_receive()) {                 // suspends while jobs is empty
        co_await results.async_send((*job)->id * 2);
    }
}

task<long> summer(channel<int>& results) {
    long sum = 0;
    while (auto r = co_await results.async_receive()) {
        sum += *r;
    }
    co_return sum;
}

int main() {
    channel<tracked_ptr<Job>> jobs(8);
    channel<int> results(8);
    vector<task<>> producers, workers;
    for (int p : range(4)) {
        producers.push_back(spawn(producer(jobs, p * 100, 100)));
    }
    for (int w : range(3)) {
        workers.push_back(spawn(worker(jobs, results)));
    }
    auto sum = spawn(summer(results));
    for (auto& p : producers) {
        p.join();                                      // this thread waits; the tasks run on the workers
    }
    jobs.close();                                      // the workers' loops end
    for (auto& w : workers) {
        w.join();
    }
    results.close();                                   // the summer's loop ends
    std::cout << sum.join() << "\n";                   // 2 * (0 + 1 + ... + 399)
    return sum.result() == 2L * 399 * 400 / 2 ? 0 : 1;
}
```

The output:

```
159600
```

## See also

- [coroutine](coroutine.md): `task`, `managed_frame`, the frames on the managed heap; [channel](channel.md): what tasks wait on
- [README: Coroutines](README.md#coroutines)
- `tests/async/scheduler.cpp`: every behaviour above, checked.
