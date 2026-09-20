# sgcl::spawn_blocking, sgcl::blocking_task, sgcl::blocking_pool

```cpp
#include "sgcl/async/blocking.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class F> auto spawn_blocking(F f);   // f on the blocking pool: a blocking_task<T> of what it returns
    template<class F> auto blocking(F f);         // the same, where it reads better
    template<class T> class blocking_task;        // the handle: co_await it, or join() it from a thread
    struct blocking_pool;                         // the pool as the program sees it: statistics, the idle time, wait_idle, stop
}
```

A blocking call from a task. A worker of the [scheduler](scheduler.md) runs every task that is ready, and a call that blocks it, a file read without the [reactor](reactor.md), `getaddrinfo`, a C library, a database driver, takes it from all of them for as long as the call lasts. `co_await spawn_blocking(f)` runs `f` on a pool of threads apart from the workers instead, threads meant to sit in the kernel, and hands back what `f` returned, or rethrows what it threw, through a [promise](promise.md): the task holds no thread while the call runs, and the workers go on with the other tasks. This is tokio's `spawn_blocking` and Java's `Executors.newCachedThreadPool` under a `co_await`; Go's answer is a goroutine whose thread the runtime replaces while it is in the call, which C++ coroutines cannot do, so the call goes to a thread that is nobody's worker.

The pool is one per process, started by the first job that finds no idle thread and grown one thread per such job up to `config::BlockingThreads` (`-DSGCL_BLOCKING_THREADS`: the larger of 64 and four times the hardware concurrency, by default, since the threads block rather than compute); a job that finds every thread busy and the pool at its cap waits in the queue for the next thread to free up. A thread that finds the queue empty parks for the idle time (`config::BlockingIdleMilliseconds`, `-DSGCL_BLOCKING_IDLE_MS`, 10 s; `blocking_pool::set_idle_time` at run time) and exits when nothing came, so that a program that stopped blocking has no threads for it. A job is a managed object holding the closure and the promise: the closure lives there, so that what it captured (a `tracked_ptr` to the buffer being read into) is traced through the job's pointer map, and the promise is where the task waits. The queue between the tasks and the pool is a `concurrent_queue` in a managed object the pool owns, as the scheduler's global queue is: a push holds the job while it waits, lock-free; the pool's mutex covers its counts only (the idle threads, the wake credits, the jobs pending, the threads themselves), taken once per job by the submitter and once per batch of jobs by a thread, which the thread wake it arbitrates costs far more than. `scheduler::stop()` stops the pool too, after the timers and the reactor.

The round trip of `co_await spawn_blocking([] {})`, a job that does nothing, is 6 to 7 µs on an Apple M2 Ultra (macOS 26, Apple clang 21, `-O2`): two hand-offs between threads through the kernel, the pool's thread woken on its condition variable and the task's worker woken by the set, against 140 ns for a promise made, set and awaited ready on one thread. A call worth the pool blocks for longer than that; a call of a few microseconds is cheaper on the worker.

## Rules

- `f` is moved into the job, a managed object: it may capture `tracked_ptr`s by value ([The rules](../core/README.md#the-rules), 1 holds: the closure is inside a managed object). What it captures by reference must outlive the job, which a task's locals do while the task awaits it; a task that drops the handle and goes on must not have lent it a reference.
- `f` runs on a thread of the pool, not a worker: it may block, and it must not `co_await` (it is not a coroutine) nor `join()` a task from where a deadlock could follow. It may `spawn_blocking` another job, which gets a thread of its own.
- The result comes back by value (moved out of the promise, once), or the exception `f` threw is rethrown by the `co_await` or the `join()`; a job whose handle nobody keeps runs all the same and its result is discarded, the job the collector's once it ran.
- A `blocking_task` holds its job through a `root_ptr`, as a `task` holds its frame: it lives anywhere, a `std::vector` of handles included, at the cost of a cell per handle; move-only. `join()` blocks the calling thread: not from a task on a worker.
- The pool's threads are threads of the program to the collector, like any other; a destructor of a collected object may run on one of them as on any.
- `blocking_pool::stop()` (and `scheduler::stop()`, and the end of the program) runs the jobs queued so far to the end and joins the threads: a job that blocks forever holds the stop forever. The next `spawn_blocking` starts the pool again.

## Members

### spawn_blocking, blocking

```cpp
template<class F> auto spawn_blocking(F f);   // blocking_task<T>, T what f returns (void for nothing)
template<class F> auto blocking(F f);         // the same
```

### blocking_task

```cpp
bool done() const noexcept;                   // the job ran (its value or exception is in)
T join();                                     // a thread waits: the result, or what f threw
awaiter operator co_await() noexcept;         // a task waits: co_await t, the same, no thread held
promise<T>& result() noexcept;                // the promise the job fills: t.result().on_ready(f) as a select case
```

### blocking_pool

```cpp
static statistics get_statistics();           // the threads, the idle ones among them, the jobs waiting
static unsigned max_threads() noexcept;       // config::BlockingThreads, resolved
static void set_idle_time(duration d);        // how long an idle thread waits for a job before it exits
static duration idle_time();
static void wait_idle();                      // blocks until every job queued so far has run
static void stop();                           // the jobs queued run to the end, the threads joined

struct statistics {
    unsigned threads;                         // the threads of the pool now (0: none, or not started)
    unsigned idle;                            // of them, parked with nothing to do
    size_t queued;                            // jobs waiting for a thread
};
```

```cpp
sgcl::task<std::string> read_file(std::string path) {
    co_return co_await sgcl::spawn_blocking([path] {              // the read on the pool: the worker is free meanwhile
        std::ifstream in(path);
        return std::string(std::istreambuf_iterator<char>(in), {});
    });
}
sgcl::task<int> resolve(std::string host) {
    co_return co_await sgcl::blocking([host] {                    // getaddrinfo blocks: not on a worker
        addrinfo* found = nullptr;
        int rc = ::getaddrinfo(host.c_str(), nullptr, nullptr, &found);
        if (rc == 0) {
            ::freeaddrinfo(found);
        }
        return rc;
    });
}
sgcl::task<bool> resolve_within(std::string host, sgcl::duration d) {
    sgcl::blocking_task<int> job = sgcl::spawn_blocking([host] { return legacy_lookup(host); });
    co_return co_await sgcl::async_select(                        // bounded: on a timeout the job runs on, its result dropped
        job.result().on_ready([] {}),
        sgcl::timeout(d, [] {})
    ) == 0;
}
sgcl::blocking_pool::statistics from_a_thread() {
    int rc = sgcl::spawn_blocking([] { return legacy_lookup("db"); }).join();   // a thread waits for the job instead
    sgcl::blocking_pool::wait_idle();                             // every job queued so far has run
    auto st = sgcl::blocking_pool::get_statistics();              // st.threads, st.idle, st.queued
    sgcl::blocking_pool::stop();                                  // the threads gone; the next spawn_blocking starts the pool again
    return rc == 0 ? st : sgcl::blocking_pool::statistics{};
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
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
int legacy_lookup(int id) {
    std::this_thread::sleep_for(20ms);
    return id * 10;
}

sgcl::task<int> lookup_all(int count) {
    sgcl::vector<sgcl::blocking_task<int>> calls;
    for (int id : sgcl::range(count)) {
        calls.push_back(sgcl::spawn_blocking([id] { return legacy_lookup(id); }));   // queued at once: a thread of the pool each
    }
    int sum = 0;
    for (auto& call : calls) {
        sum += co_await call;                                                        // suspended until the call returns
    }
    co_return sum;
}

sgcl::task<> heartbeat(std::atomic<bool>& done, std::atomic<int>& beats) {
    while (!done) {
        co_await sgcl::sleep(2ms);
        ++beats;
    }
}

int main() {
    std::atomic<bool> done = {false};
    std::atomic<int> beats = {0};
    auto pulse = sgcl::spawn(heartbeat(done, beats));
    auto start = std::chrono::steady_clock::now();
    int sum = sgcl::spawn(lookup_all(50)).join();
    bool together = std::chrono::steady_clock::now() - start < 500ms;   // fifty sequential calls would take a second
    done = true;
    pulse.join();
    auto pool = sgcl::blocking_pool::get_statistics();
    std::cout << "sum " << sum << ", the calls ran together: " << (together ? "yes" : "no")
              << ", the heartbeat kept beating: " << (beats > 0 ? "yes" : "no")
              << ", threads of the pool: " << pool.threads << "\n";
    sgcl::scheduler::stop();                                             // the workers, the timer thread and the pool joined
    return sum == 12250 && together && beats > 0 ? 0 : 1;
}
```

The output:

```
sum 12250, the calls ran together: yes, the heartbeat kept beating: yes, threads of the pool: 50
```

## See also

- [promise](promise.md): what carries the result back; [scheduler](scheduler.md): the workers the pool keeps free; [reactor](reactor.md): the wait for a descriptor that needs no thread at all, the better tool for a socket
- [config](../core/config.md): `SGCL_BLOCKING_THREADS`, `SGCL_BLOCKING_IDLE_MS`
- `tests/async/blocking.cpp`: every behaviour above, checked.
