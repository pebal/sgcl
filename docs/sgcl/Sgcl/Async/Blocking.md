# SpawnBlocking, BlockingTask, BlockingPool

```cpp
#include "sgcl/Sgcl/Async/Blocking.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class F> auto SpawnBlocking(F f);   // f on the blocking pool: a BlockingTask<T> of what it returns
    template<class F> auto Blocking(F f);        // the same, where it reads better
    template<class T> class BlockingTask;        // the handle: co_await it, or Join() it from a thread
    struct BlockingPool;                         // the pool as the program sees it: statistics, the idle time, WaitIdle, Stop
}
```

The same in the `sgcl` interface: [spawn_blocking, blocking_task, blocking_pool](../../async/blocking.md).

A blocking call from a task. A worker of the [Scheduler](Scheduler.md) runs every task that is ready, and a call that blocks it, a file read without the [reactor](Reactor.md), `getaddrinfo`, a C library, a database driver, takes it from all of them for as long as the call lasts. `co_await SpawnBlocking(f)` runs `f` on a pool of threads apart from the workers instead, threads meant to sit in the kernel, and hands back what `f` returned, or rethrows what it threw, through a [Promise](Promise.md): the task holds no thread while the call runs, and the workers go on with the other tasks. This is tokio's `spawn_blocking` and Java's `Executors.newCachedThreadPool` under a `co_await`; Go's answer is a goroutine whose thread the runtime replaces while it is in the call, which C++ coroutines cannot do, so the call goes to a thread that is nobody's worker.

The pool is one per process, started by the first job that finds no idle thread and grown one thread per such job up to `config::BlockingThreads` (`-DSGCL_BLOCKING_THREADS`: the larger of 64 and four times the hardware concurrency, by default, since the threads block rather than compute); a job that finds every thread busy and the pool at its cap waits in the queue for the next thread to free up. A thread that finds the queue empty parks for the idle time (`config::BlockingIdleMilliseconds`, `-DSGCL_BLOCKING_IDLE_MS`, 10 s; `BlockingPool::SetIdleTime` at run time) and exits when nothing came, so that a program that stopped blocking has no threads for it. A job is a managed object holding the closure and the promise: the closure lives there, so that what it captured (a `Ptr` to the buffer being read into) is traced through the job's pointer map, and the promise is where the task waits. The queue between the tasks and the pool is a `ConcurrentQueue` in a managed object the pool owns, lock-free; the pool's mutex covers its counts only. `Scheduler::Stop()` stops the pool too, after the timers and the reactor.

The round trip of `co_await SpawnBlocking([] {})`, a job that does nothing, is 6 to 7 µs on an Apple M2 Ultra (macOS 26, Apple clang 21, `-O2`): two hand-offs between threads through the kernel, the pool's thread woken on its condition variable and the task's worker woken by the set, against 140 ns for a promise made, set and awaited ready on one thread. A call worth the pool blocks for longer than that; a call of a few microseconds is cheaper on the worker.

## Rules

- `f` is moved into the job, a managed object: it may capture `Ptr`s by value ([The rules](../../core/README.md#the-rules), 1 holds: the closure is inside a managed object). What it captures by reference must outlive the job, which a task's locals do while the task awaits it; a task that drops the handle and goes on must not have lent it a reference.
- `f` runs on a thread of the pool, not a worker: it may block, and it must not `co_await` (it is not a coroutine) nor `Join()` a task from where a deadlock could follow. It may `SpawnBlocking` another job, which gets a thread of its own.
- The result comes back by value (moved out of the promise, once), or the exception `f` threw is rethrown by the `co_await` or the `Join()`; a job whose handle nobody keeps runs all the same and its result is discarded, the job the collector's once it ran.
- A `BlockingTask` holds its job through a `RootPtr`, as a `Task` holds its frame: it lives anywhere, a `List` or a `std::vector` of handles included, at the cost of a cell per handle; move-only. `Join()` blocks the calling thread: not from a task on a worker.
- The pool's threads are threads of the program to the collector, like any other; a destructor of a collected object may run on one of them as on any.
- `BlockingPool::Stop()` (and `Scheduler::Stop()`, and the end of the program) runs the jobs queued so far to the end and joins the threads: a job that blocks forever holds the stop forever. The next `SpawnBlocking` starts the pool again.

## Members

### SpawnBlocking, Blocking

```cpp
template<class F> auto SpawnBlocking(F f);   // BlockingTask<T>, T what f returns (void for nothing)
template<class F> auto Blocking(F f);        // the same
```

### BlockingTask

```cpp
bool IsDone() const noexcept;                // the job ran (its value or exception is in)
T Join();                                    // a thread waits: the result, or what f threw
auto operator co_await() noexcept;           // a task waits: co_await t, the same, no thread held
sgcl::promise<T>& Result() noexcept;         // the promise the job fills: t.Result().on_ready(f) as a Select case
sgcl::blocking_task<T>& Inner() noexcept;
```

### BlockingPool

```cpp
static Statistics GetStatistics();           // the threads, the idle ones among them, the jobs waiting
static unsigned MaxThreads() noexcept;       // config::BlockingThreads, resolved
static void SetIdleTime(Duration d);         // how long an idle thread waits for a job before it exits
static Duration IdleTime();
static void WaitIdle();                      // blocks until every job queued so far has run
static void Stop();                          // the jobs queued run to the end, the threads joined

struct Statistics {
    unsigned Threads;                        // the threads of the pool now (0: none, or not started)
    unsigned Idle;                           // of them, parked with nothing to do
    size_t Queued;                           // jobs waiting for a thread
};
```

```cpp
Task<std::string> ReadFile(std::string path) {
    co_return co_await SpawnBlocking([path] {                     // the read on the pool: the worker is free meanwhile
        std::ifstream in(path);
        return std::string(std::istreambuf_iterator<char>(in), {});
    });
}
Task<int> Resolve(std::string host) {
    co_return co_await Blocking([host] {                          // getaddrinfo blocks: not on a worker
        addrinfo* found = nullptr;
        int rc = ::getaddrinfo(host.c_str(), nullptr, nullptr, &found);
        if (rc == 0) {
            ::freeaddrinfo(found);
        }
        return rc;
    });
}
Task<bool> ResolveWithin(std::string host, Duration d) {
    BlockingTask<int> job = SpawnBlocking([host] { return LegacyLookup(host); });
    co_return co_await AsyncSelect(                               // bounded: on a timeout the job runs on, its result dropped
        job.Result().on_ready([] {}),
        Timeout(d, [] {})
    ) == 0;
}
BlockingPool::Statistics FromAThread() {
    int rc = SpawnBlocking([] { return LegacyLookup("db"); }).Join();   // a thread waits for the job instead
    BlockingPool::WaitIdle();                                     // every job queued so far has run
    auto st = BlockingPool::GetStatistics();                      // st.Threads, st.Idle, st.Queued
    BlockingPool::Stop();                                         // the threads gone; the next SpawnBlocking starts the pool again
    return rc == 0 ? st : BlockingPool::Statistics{};
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

// A synchronous library: a call that holds its thread for 20 ms (a
// database driver, getaddrinfo, a file read). Fifty of them at once from
// a task, on the blocking pool, while a heartbeat task keeps running on
// the workers: the calls take 20 ms together, not one after another, and
// the workers are never held.
int LegacyLookup(int id) {
    std::this_thread::sleep_for(20ms);
    return id * 10;
}

Task<int> LookupAll(int count) {
    List<BlockingTask<int>> calls;
    for (int id : Range(count)) {
        calls.Add(SpawnBlocking([id] { return LegacyLookup(id); }));   // queued at once: a thread of the pool each
    }
    int sum = 0;
    for (auto& call : calls) {
        sum += co_await call;                                          // suspended until the call returns
    }
    co_return sum;
}

Task<> Heartbeat(std::atomic<bool>& done, std::atomic<int>& beats) {
    while (!done) {
        co_await Sleep(2ms);
        ++beats;
    }
}

int main() {
    std::atomic<bool> done = {false};
    std::atomic<int> beats = {0};
    auto pulse = Spawn(Heartbeat(done, beats));
    auto start = std::chrono::steady_clock::now();
    int sum = Spawn(LookupAll(50)).Join();
    bool together = std::chrono::steady_clock::now() - start < 500ms;   // fifty sequential calls would take a second
    done = true;
    pulse.Join();
    auto pool = BlockingPool::GetStatistics();
    std::cout << "sum " << sum << ", the calls ran together: " << (together ? "yes" : "no")
              << ", the heartbeat kept beating: " << (beats > 0 ? "yes" : "no")
              << ", threads of the pool: " << pool.Threads << "\n";
    Scheduler::Stop();                                                   // the workers, the timer thread and the pool joined
    return sum == 12250 && together && beats > 0 ? 0 : 1;
}
```

The output:

```
sum 12250, the calls ran together: yes, the heartbeat kept beating: yes, threads of the pool: 50
```

## See also

- [Promise](Promise.md): what carries the result back; [Scheduler](Scheduler.md): the workers the pool keeps free; [Readable, Writable](Reactor.md): the wait for a descriptor that needs no thread at all, the better tool for a socket
- [Config](../../core/config.md): `SGCL_BLOCKING_THREADS`, `SGCL_BLOCKING_IDLE_MS`
- `tests/Sgcl/promise_and_blocking.cpp`: the behaviour above, checked.
