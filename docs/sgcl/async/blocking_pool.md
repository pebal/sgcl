[sgcl](../README.md) › [async](README.md)

# sgcl::async::blocking_pool

```cpp
#include "sgcl/async/blocking.h"   // or "sgcl/async.h"

namespace sgcl::async {
    struct blocking_pool;
}
```

`sgcl::async::blocking_pool` is the program's view of the pool of threads that run the jobs of
[spawn_blocking](spawn_blocking.md) and [go_blocking](go_blocking.md): threads apart from the
[scheduler](scheduler.md)'s workers, meant to sit in the kernel in a blocking call while the workers go on with the
tasks. There is one pool per process, and every member is static: the counts, the cap on the threads, how long an idle
thread stays, a wait for every job queued, a stop.

The pool is started by the first job that finds no idle thread, and grows by one thread per such job up to its cap;
a job that finds every thread busy and the pool at its cap waits in the queue for the next thread to free up, and so
does one whose thread the system refuses to start while another thread is there (with none, `spawn_blocking`
throws `std::system_error` and the job never runs). The cap
is the program's ([set_threads](blocking_pool/set_threads.md)), else `SGCL_BLOCKING_THREADS` from the environment
(read once, at the first job), else `config::blocking_threads` (`-DSGCL_BLOCKING_THREADS`); 0 in any of them is the
larger of 64 and four times the hardware concurrency, since the threads block rather than compute. A thread that
finds the queue empty parks for the idle time (`config::blocking_idle_milliseconds`, 10 s;
[set_idle_time](blocking_pool/set_idle_time.md)) and exits when nothing came, so that a program that stopped blocking
has no threads for it.

The queue between the tasks and the pool is a `concurrent::queue` in a managed object the pool owns, as the
scheduler's global queue is: a push holds the job while it waits, lock-free. The pool's mutex covers its counts only
(the idle threads, the wake credits, the jobs pending, the threads themselves), taken once per job by the submitter
and once per batch of jobs by a thread, which the thread wake it arbitrates costs far more than.

## Rules

- A job is a managed object holding the closure and the promise of its result: the closure may hold `tracked_ptr`s,
  traced through the job ([spawn_blocking](spawn_blocking.md)).
- The pool's threads are threads of the program to the collector, like any other; a destructor of a collected object
  may run on one of them as on any. Before a thread parks it clears the dead part of its stack, so the words a job
  left there keep nothing alive.
- [stop](blocking_pool/stop.md), [scheduler::stop](scheduler/stop.md) and the end of the program run the jobs queued
  so far to their end and join the threads: a job that blocks forever holds the stop forever. The next
  `spawn_blocking` starts the pool again.
- [wait_idle](blocking_pool/wait_idle.md) and [stop](blocking_pool/stop.md) block the calling thread: not from a
  task on a worker (debug builds assert).
- A value of `SGCL_BLOCKING_THREADS` that does not read is ignored with one line on stderr, and the default taken.

## Member types

| Type | Definition |
|---|---|
| [statistics](blocking_pool-statistics.md) | the threads, the idle ones, the jobs waiting, what `get_statistics` returns |

## Member functions

#### Observers

| Function | Description |
|---|---|
| [get_statistics](blocking_pool/get_statistics.md) | the threads, the idle ones, the jobs waiting (static) |
| [max_threads](blocking_pool/max_threads.md) | the most threads the pool grows to (static) |
| [idle_time](blocking_pool/idle_time.md) | how long an idle thread waits for a job before it exits (static) |

#### Modifiers

| Function | Description |
|---|---|
| [set_threads](blocking_pool/set_threads.md) | sets the most threads the pool grows to (static) |
| [set_idle_time](blocking_pool/set_idle_time.md) | sets how long an idle thread waits for a job (static) |

#### Operations

| Function | Description |
|---|---|
| [wait_idle](blocking_pool/wait_idle.md) | waits until every job queued so far has run (static) |
| [stop](blocking_pool/stop.md) | runs the jobs queued to their end and joins the threads (static) |

## Complexity

A job: a push on the lock-free queue and the pool's lock taken once, then a wake of an idle thread through the
kernel, or the start of a thread. A thread runs the jobs it finds one after another and takes the lock once per batch.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int slow_call(int x) {  // holds its thread for 50 ms
    this_thread::sleep_for(50ms);
    return x;
}

async::task<int> calls() {
    vector<async::blocking_task<int>> jobs;
    for (int i : range(4)) {
        jobs.push_back(async::spawn_blocking([i] { return slow_call(i); }));
    }
    int sum = 0;
    for (auto& job : jobs) {
        sum += co_await job;  // the worker is free meanwhile
    }
    co_return sum;
}

async::task<> other() {
    println("another task runs while the calls block");
    co_return;
}

int main() {
    async::scheduler::set_workers(1);  // one worker for both tasks
    auto a = async::spawn(calls());
    auto b = async::spawn(other());
    println("sum {}", a.wait());
    b.wait();
    println("threads of the pool: {}", async::blocking_pool::get_statistics().threads);
}
```

Output:

```text
another task runs while the calls block
sum 6
threads of the pool: 4
```

## See also

- [spawn_blocking](spawn_blocking.md), [blocking_task](blocking_task.md): a job and its handle
- [go_blocking](go_blocking.md): a job with no handle
- [scheduler](scheduler.md): the workers the pool keeps free
- [config](../core/config.md): `SGCL_BLOCKING_THREADS`, `SGCL_BLOCKING_IDLE_MS`
