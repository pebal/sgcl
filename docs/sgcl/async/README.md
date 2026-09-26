# sgcl::async

What Go has around goroutines: coroutine frames on the managed heap and the tasks over them, the pool of workers that runs them, channels and select, time and cancellation, the composition and the synchronization of tasks, the readiness of a file descriptor as a channel. Each of them is waited for the same three ways: a thread blocks, a task `co_await`s, a select takes it as a case. `#include "sgcl/async/async.h"` brings the module in; it depends on [`core`](../core/README.md) and [`concurrent`](../concurrent/README.md), and [`io`](../io/README.md) (the async forms of its streams: the blocking pool for a file, the reactor for a pipe or a socket) and [`net`](../net/README.md) are built on it; the index of the whole interface is [`docs/sgcl/`](../README.md).


## Waiting operations

Every operation of this module that may wait — a channel's `send` and `receive`, a semaphore's `acquire`, a condition variable's `wait(guard)`, a mutex's `scoped_lock` — has one name and returns an `async::operation`: a description of the operation, marked nodiscard, which does nothing until it is carried out in one of two ways (`select` returns an object of its own, nodiscard too, carried out the same two ways). An event, a wait group and a task group are waited for as a task is: `wait()`, which returns nothing, blocks a thread, and a task writes `co_await e` on the object itself. The modules built on this one (io, net, encoding, hash) do not return an `async::operation`: each of their operations has two names, `read(...)`, which does the work on the calling thread, and `async_read(...)`, which returns an `async::task` for a task to `co_await`.

```cpp
auto v = co_await ch.receive();      // in a task: the task suspends, the worker runs others
auto w = ch.receive().wait();        // on a thread: the thread blocks until a value comes
```

`co_await` is for a coroutine and gives the worker back while the operation waits; `.wait()` is for code that is not a coroutine (`main`, a `std::thread`) and blocks the calling thread, going straight to the operation's own blocking code, never through the scheduler. A `.wait()` inside a task would hold a worker, and so is never written there. A task, a promise, a job of the blocking pool are waited for themselves, and the wait gives the result, as an operation's does: `co_await t` in a task, `t.wait()` on a thread (the way `std::this_thread::sync_wait` gives a sender's); `t.result()` reads the result of one that is done. The Lockable members the standard names (`lock`, `try_lock`, `unlock`, `lock_shared`...) stay blocking, for `std::lock_guard` and `std::shared_lock`.

## Threads
Any thread may create and copy managed pointers; the library registers a thread's stack the first time the thread copies one, and forgets it when the thread exits, with a handshake so that a scan in progress finishes reading the stack first. The collector runs on a thread of its own, started with the first managed object, and on a pool of helpers that park between cycles; destructors of collected objects run on those threads, so a destructor must be prepared to run on a thread other than the one that created the object, and must not touch the dying peers of its object (rule 5). Nothing a mutator does waits for a cycle, and nothing a cycle does waits for a mutator: a thread blocked in a system call, spinning, or descheduled holds up no one; a thread that exits mid-scan waits for the scan of its own stack only.

A `fork()` gives the child a copy-on-write snapshot of the managed heap and none of the threads: the child may read managed objects and `exec` or exit (the page headers live outside the pages, so the parent's marking does not copy the snapshot page by page), but the collector does not run in it, and a managed allocation that needs a page or a call into the collector terminates the child with a message rather than hang on a copied lock. A child that needs the collector is a `fork` before the first managed object, or an `exec`.

## Coroutines
The frame of a C++20 coroutine, where its parameters, locals and promise live between suspensions, is allocated with `operator new`: heap memory the collector does not see, so a `tracked_ptr` in a coroutine breaks rule 1 and its object may be collected under it. A promise type that derives from `sgcl::managed_frame` gets its frames from the managed heap instead: `operator new` of the promise allocates the frame as a buffer of words on the managed heap, and the collector traces such a buffer conservatively, every word that holds a managed address keeping its object, so whatever the coroutine holds is a root while its frame is held. The frame is held through a `frame_ptr<Promise>` (a `root_ptr` to the frame and the coroutine handle, move-only, destroys the coroutine when destroyed) that the promise's `get_return_object` makes from the handle. The frame and its owner are the core's ([managed_frame, frame_ptr](../core/coroutine.md)), and so is `generator<T>` ([generator](../core/generator.md)), which runs where it is called; `async::task<T>` is this module's, and keeps its executor, its task-locals and the link of an executor's queue in the four words the core leaves in front of every managed frame.

`async::task<T>` is what runs on the scheduler: `sgcl::async::spawn(f())` puts the task on the queue of the ready and returns at once, a worker of the pool (one per core) runs it to its next `co_await`, and whatever it waited for, a `channel`, another task, `sgcl::async::yield()`, makes it ready again. A task that waits is nowhere: a frame on the managed heap and a word on a list, no thread held, so a hundred thousand tasks waiting on a channel cost their frames and nothing else, and a `send` to a waiting task is a push on the scheduler's queue (a worker woken through the kernel only when the whole pool sleeps). A thread waits for a task with `wait()`, a task with `co_await`; a task nobody waits for is started with `sgcl::async::go(f())` (a spawn whose handle nobody keeps; `spawn` is `[[nodiscard]]`, since a task object dropped destroys the coroutine) and destroys its frame itself when it is done, the memory the collector's from then on. This is the model of Go with the two differences of C++: tasks are stackless (only the body of a coroutine can suspend, a function it calls cannot) and cooperative (a task that computes for a second holds its worker for a second).

```cpp
struct Job { int id; };

async::task<> worker(async::channel<tracked_ptr<Job>>& jobs, async::channel<int>& results) {
    while (auto job = co_await jobs.receive()) {   // suspended while jobs is empty: no thread held
        co_await results.send((*job)->id * 2);     // suspended while results is full
    }
}

async::task<long> summer(async::channel<int>& results) {
    long sum = 0;
    while (auto r = co_await results.receive()) sum += *r;
    co_return sum;
}

async::channel<tracked_ptr<Job>> jobs(8);
async::channel<int> results(8);
for (int w : range(4)) async::go(worker(jobs, results));   // four workers, on the pool
auto sum = async::spawn(summer(results));
for (int i : range(1000)) jobs.send(make_tracked<Job>(i)).wait();      // from this thread: each send wakes a worker task
jobs.close(); /* ... */ results.close();
long total = sum.result();                                                  // this thread waits; or co_await sum from a task
```

A task that waits on several channels writes a `select`: one case per channel, a receive or a send with a body, and the first case its channel can serve runs its body; `otherwise` is the case taken when none can be served at once, which makes the select a poll. A thread writes the same with `async::select(...)` and blocks. The loop of a worker that serves a queue until told to stop:

```cpp
async::task<> worker(async::channel<tracked_ptr<Job>>& jobs, async::channel<void>& stop) {
    bool running = true;
    while (running) {
        co_await async::select(
            jobs.on_receive([&](tracked_ptr<Job> job) { handle(job); }),   // an element came
            stop.on_receive([&] { running = false; })                            // a signal, or stop closed
        );
    }
}
```

Around the tasks, what Go has around goroutines, each a channel under another name so that it is waited for the same three ways (a thread blocks, a task `co_await`s, a select takes it as a case): time (`co_await async::sleep(d)` and `async::sleep_until(t)`, `async::after(d)` and `at(t)`, `async::tick(d)`, `async::timeout(d, f)` in a select; one timer thread with a heap under them), the signals of the process (`async::signals({SIGINT, SIGTERM})`: a channel of the numbers delivered, Go's `os/signal.Notify`, the shutdown of a server one more case of its select), cancellation (`stop_source` and `stop_token`: a token is a channel closed by the stop, `token.on_stop(f)` a case of a select, `source.stop_after(d)` or `source.stop_at(t)` a deadline, a source made from a token a child stopped with its parent), the composition of tasks (`when_all` giving every result, `when_any` the first to finish; a task nobody spawned starts with the first wait for it), the synchronization of tasks with each other and with threads (`mutex`, `semaphore`, `event`, `wait_group`, `once`, none of which parks a worker; `once` is the one of them that is no case of a select), the readiness of a descriptor as a channel (`async::readable(fd)`, `async::writable(fd)`: one thread on kqueue under them, what the [io](../io/README.md) and [net](../net/README.md) modules wait on), and `async::generator<T>`, a generator that may wait between the values it yields. The reference: [select](select.md), [timer](timer.md), [signal](signal.md), [stop_token](stop_token.md), [when](when.md), [mutex](mutex.md) and its family, [reactor](reactor.md).

The module reads the time in one place, `sgcl::clock::now()`, so that a test can own it: `sgcl::async::manual_clock c; c.install();` stops the module's time where the steady clock was, and `c.advance(30s)` moves it thirty seconds and fires every timer due by then, in order, returning when the tasks woken have reached their next waits; a test of a thirty-second timeout, a retry every minute or a deadline takes microseconds and is deterministic, the thing tokio's `time::pause()`/`advance()` and Kotlin's `runTest` give. The production path pays one relaxed load of a flag per read of the time. [clock](manual_clock.md).

Two more for the world outside the tasks. A `async::promise<T>` is a completion that any thread sets once, `p.set_value(v)`, and a task awaits, `co_await p`: the adapter between the callback APIs of a platform (an I/O completion port, a dispatch queue, JNI, a C library with a `void*` context) and `co_await`, an event with a value, so that it too is waited for the three ways (a thread's `wait()`, a select's `p.on_done(f)`); Java's `CompletableFuture`, Kotlin's `CompletableDeferred`, Rust's `oneshot`. And a call that blocks (a file read without the reactor, `getaddrinfo`, a database driver) must not run on a worker, which it would take from every task: `co_await async::spawn_blocking(f)` runs it on a pool of threads apart from the workers, threads meant to sit in the kernel, started on demand up to a cap and gone after an idle time, and hands the result back through a promise; tokio's `spawn_blocking`, where Go's runtime hands the blocked goroutine's thread to another. The reference: [promise](promise.md), [blocking](blocking.md).
A task runs on whichever worker is free, and a platform's UI toolkit (Cocoa, Win32, GTK, the browser) demands one thread, usually the main one, for every call into it. An `executor` is a queue of tasks that one thread runs, the thread of the program's choosing: `ex.run()` is that thread's loop, `ex.run(task)` the loop until the task is done (`int main() { sgcl::async::executor main; return main.run(program()); }`: the program one task on the main thread), `ex.poll()` one pass for a loop that is not the library's (a `CFRunLoop`, a `GMainLoop`, a game's frame). `co_await on(ex)` moves a task there and `co_await on_workers()` back to the pool, Kotlin's `withContext(Dispatchers.Main)` and `withContext(Dispatchers.Default)`; a task on an executor is resumed there after every wait, whatever thread served it, because its frame remembers the executor and every wake goes through the scheduler's one `enqueue`. A `strand` is an executor with no thread of its own, Boost.Asio's strand: its tasks run on the workers, never two at once, in order, serial access to a resource without a lock. A `async::task_local<T>` is a value a task sets (`co_await request_id.set(7)`) and every function and task under it reads (`request_id.get()`), inherited by the tasks it starts and not by its siblings, Go's `context.WithValue`, Kotlin's `CoroutineContext`, tokio's `task_local!`; the values are managed nodes chained from a word of the frame, so inheritance is a word copied and a child's set is a node of its own. The reference: [executor](executor.md), [task_local](task_local.md).
Over these, the two shapes that keep a set of tasks in a scope: `task_group`, Go's `errgroup` and Kotlin's `coroutineScope` (Java's `StructuredTaskScope`), a scope made under a token that owns the tasks it spawns, waited for as one (`g.wait()` on a thread, `co_await g`, `g.on_done(f)` in a select) and stopped as one, by the first child that throws (the others see it through `g.token()`, the wait rethrows that exception once every child has finished), by `request_stop()`, by the parent's token, or by the scope's end; and a timeout on one task, Go's `context.WithTimeout` and `WithDeadline`: `co_await async::with_timeout(t, d)` (or `with_deadline(t, when)`) is the result or the error `timed_out`, an `expected` as every failure of the library is, a race of the task and a timer, with the loser stopped through a source given as the third argument or left to finish on its own. The reference: [task_group](task_group.md), [timeout](timeout.md).
Two more of the synchronization are not channels under a name but words with channels for their waits: `shared_mutex`, Go's `sync.RWMutex` and Java's `ReentrantReadWriteLock` (any number of readers at once, or one writer; a reader's lock is an atomic add, a few times cheaper than the channel-built `mutex`; a writer waiting blocks the readers that come after it and the readers it held back go before the next writer, so neither side starves), and `condition_variable`, Go's `sync.Cond` over the module's `mutex` (`co_await cv.wait(guard, predicate)`: the mutex let go of, the wait, the mutex taken back with a `co_await` too; a notify before the wait began is lost, as everywhere, so the condition is checked under the mutex before every wait). And a channel of another shape: `async::broadcast<T>`, tokio's `broadcast` and Kotlin's `SharedFlow`, the event bus of the ui module, where every subscription receives every value: one ring shared by all, a cursor per subscription, a send that never waits, a subscriber that falls more than the capacity behind lapped and told how many values it lost (`lagged()`), a value held by the ring until every subscription has passed it, and the three ways of receiving (`s.receive()`, `co_await s.receive()`, `s.on_receive(f)` in a select). The reference: [mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md), [shared_mutex](shared_mutex.md), [condition_variable](condition_variable.md), [broadcast](broadcast.md).

`generator<T>` (the core's) is a coroutine that `co_yield`s values, consumed with a range-for; a `task` may also be driven by hand, `resume()` one step at a time, without the scheduler:

```cpp
generator<tracked_ptr<Node>> chain(int count) {
    tracked_ptr<Node> last;                 // a local in the frame: a root while suspended
    for (int i : range(count)) {
        tracked_ptr n = make_tracked<Node>(i, last);
        last = n;
        co_yield n;                               // suspended here, the chain is alive
    }
}
for (auto& n : chain(5)) { /* the generator's frame holds the chain */ }
```

The rules that follow: a `task` or `generator` (a `frame_ptr`) lives anywhere, a `std::vector<async::task<>>` included, at the cost of a cell per handle; the coroutine is destroyed when its `frame_ptr` is, which runs the destructors of its locals and promise on the calling thread, and the frame's memory goes to the collector once nothing holds it, so no `coroutine_handle` may outlive the `frame_ptr`; a waiting coroutine is held by what it waits on. On a worker a task waits with `co_await`, never with a blocking call, which would take the worker from every other task. The cost is the allocation of the frame as a managed buffer (a few tens of nanoseconds instead of `malloc`) and, per cycle, a conservative pass over the frame's words; a frame that holds no managed pointers costs the collector the same pass and nothing else. The scheduler is one per process, started by the first `spawn`, stopped when the program ends or by `async::scheduler::stop()`; `-DSGCL_WORKERS` sets the number of workers ([docs/sgcl/scheduler.md](scheduler.md)).

## Pages

| page | header | what it is |
|---|---|---|
| [coroutine](coroutine.md) | `coroutine.h`, `generator.h` | `task`, `async::generator`: coroutines on the managed frame of the core ([managed_frame, frame_ptr](../core/coroutine.md), whose locals and parameters are roots); a task is spawned, joined, awaited, detached; a task starts with the first wait for it |
| [scheduler](scheduler.md) | `scheduler.h` | the pool of workers that runs the tasks: `spawn`, `go`, `yield`, `async::scheduler::stop`; a task that waits holds no thread |
| [executor](executor.md) | `executor.h` | `executor`: a task on a thread of the program's choosing (the main thread, a foreign loop through `poll`), resumed there after every wait; `strand`: tasks on the workers one at a time, in order; `co_await on(ex)`, `co_await on_workers()` |
| [task_local](task_local.md) | `task_local.h` | a value visible to a task and to the tasks it starts, read from any function under it: `co_await x.set(v)`, `x.get()`, `x.with(v, t)`; inherited, copy on write |
| [channel](channel.md) | `channel.h` | the channel of Go: a buffered or rendezvous queue that threads and coroutines send to and receive from, waiting on either side, closed to end the stream |
| [select](select.md) | `select.h` | the select of Go: a wait on several channels at once, a receive or a send per case with a body, `otherwise` for a poll; for a thread or a coroutine |
| [broadcast](broadcast.md) | `broadcast.h` | a channel every subscriber receives every value from (tokio's broadcast, Kotlin's SharedFlow): one ring, a cursor per subscription, a send that never waits, a subscriber that falls behind lapped and told how many it lost |
| [timer](timer.md) | `timer.h` | time: `sleep`, `sleep_until`, `after`, `at`, `tick`, `timeout`: a task suspended for a while or until a point, a channel signalled once after a while or at a point or every while, a select case served after a while; one timer thread under them |
| [manual_clock](manual_clock.md) | `timer.h` | `manual_clock`, the clock of a test, over core's [`clock::now()`](../core/clock.md): installed, time moves only by `advance(d)`, which fires every timer due with no real waiting |
| [signal](signal.md) | `signal.h` | `async::signals({SIGINT, SIGTERM})`: the signals of the process as a channel, for a task, a thread or a select; `reset_signals`, `ignore_signals` |
| [stop_token](stop_token.md) | `stop_token.h` | cancellation: `stop_source` requests the stop, `stop_token` is a channel closed by it (a select case, an awaitable), a deadline is a timer, a child source stops with its parent |
| [when](when.md) | `when.h` | the composition of tasks: `when_all` (every result as a tuple or a vector), `when_any` (the index of the first to finish) |
| [mutex](mutex.md) | `mutex.h` | one holder at a time, a channel holding one signal: blocking, awaitable and as a select case; a task waiting holds no thread |
| [semaphore](semaphore.md) | `semaphore.h` | n permits, a channel holding n signals; the three forms of a wait |
| [event](event.md) | `event.h` | set once, waited for by any number: a channel closed by the set |
| [wait_group](wait_group.md) | `wait_group.h` | counts work down to zero, a channel per round closed at zero (Go's WaitGroup) |
| [once](once.md) | `once.h` | the first caller runs it, the others wait for it, blocking or awaitable |
| [shared_mutex](shared_mutex.md) | `shared_mutex.h` | any number of readers or one writer, over a word, writer preference (Go's RWMutex); blocking and awaitable |
| [condition_variable](condition_variable.md) | `condition_variable.h` | Go's Cond over the mutex: a wait lets go of the mutex, waits for a notify and takes it back; blocking and awaitable |
| [promise](promise.md) | `promise.h` | `async::promise<T>`: a one-shot completion set once by any thread or C callback, awaited by a task, blocked on by a thread, a case of a select; the adapter between a platform's callbacks and `co_await` |
| [blocking](blocking.md) | `blocking.h` | `spawn_blocking`: a blocking call on a pool of threads apart from the workers, its result back through a promise; the pool grows on demand to a cap and its idle threads exit; `blocking_pool` for its statistics and stop |
| [task_group](task_group.md) | `task_group.h` | structured concurrency: a scope that owns the tasks it spawns, waited for as one (a thread, a task, a select case), stopped as one by the first exception, which the wait rethrows; Go's `errgroup`, Kotlin's `coroutineScope` |
| [timeout](timeout.md) | `timeout.h` | a timeout on a task: `async::with_timeout(t, d)` and `with_deadline(t, when)` the result or the error `timed_out`, a token as the deadline (`stopped`); the loser stopped through its source or left to finish |
| [reactor](reactor.md) | `reactor.h` | `readable`, `writable`: the readiness of a file descriptor as a channel, one thread on the kernel's queue (kqueue; epoll and IOCP to come); the foundation of io and net |
| [benchmarks](benchmarks.md) | `benchmarks/async/async.cpp` | what a hop, a wait and a race cost on the scheduler, measured: yield, the executor, a strand, an awaited task, `when_all`, `timeout`, `select`, the condition variable, a rendezvous, the generator, the mutex |
