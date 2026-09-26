# sgcl::async::executor, sgcl::async::strand, sgcl::async::on, sgcl::async::on_workers

```cpp
#include "sgcl/async/executor.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class executor;      // a queue of tasks that one thread runs: the thread of the program's choosing
    class strand;        // an executor with no thread of its own: its tasks run on the workers, one at a time, in order
    class on;            // co_await on(ex): the task goes on on the executor or the strand
    struct on_workers;   // co_await on_workers(): the task goes on on the pool
    template<class T, class Executor> async::task<T> async::spawn(async::task<T> t, Executor& ex);   // ex.spawn(t)
    template<class T, class Executor> void async::go(async::task<T> t, Executor& ex);         // ex.go(t)
}
```

The [scheduler](scheduler.md) runs a task on whichever worker is free. A platform's UI toolkit (Cocoa, Win32, GTK, the browser) and many a C library demand one thread, usually the main one, for every call into them; an `executor` is a queue of frames that only the thread running the executor resumes. `ex.run()` is that thread's loop: what is queued, run to its next suspension; nothing queued, parked; until `stop()`. `ex.run(task)` is the loop until the task is done, and the task's result, which makes a program one task on the main thread: `int main() { sgcl::async::executor main; return main.run(program()); }`. `ex.poll()` is one pass over what is queued and a return, for a loop that is not the library's, a `CFRunLoop`, a `GMainLoop`, a game's frame, which calls it when it has a moment. A task moves to an executor with `co_await sgcl::async::on(ex)` (Kotlin's `withContext(Dispatchers.Main)`) and back to the pool with `co_await sgcl::async::on_workers()` (`withContext(Dispatchers.Default)`); `ex.spawn(t)` starts a task on it.

A task on an executor stays on it. Its frame remembers the executor (a word of the header at the front of every managed frame's buffer, `detail::FrameHeader`: the executor, the task-locals, and the link of the executor's queue), and every wake of the frame, whatever woke it, a channel, a timer, a task it awaited, a `yield`, a `select`, an event, goes through the scheduler's `enqueue`, which routes the frame to the executor's queue when it has one. So a task that does `co_await async::sleep(1s)` on the main thread is resumed on the main thread, though the timer fired on the timer thread; a task on the main thread that awaits a channel is resumed on the main thread, though a worker served it. A task that the task awaits (`co_await f()` of a task nobody started) runs where the awaiter runs, as a call would: on the main thread for a main-thread task, on the strand for a strand's task, so that what the awaiter guarantees holds for what it awaits; a task it starts with `sgcl::async::spawn` or `sgcl::async::go` runs on the pool, one it starts with `ex.spawn` or `ex.go` on the executor.

A `strand` is an executor with no thread of its own (Boost.Asio's strand): its tasks run on the workers, never two at once, in the order they were queued. Serial access to something without a lock: the handler of a connection, the owner of a document, a log three tasks append to. The strand is a queue whose head is handed to the workers when the strand goes from idle to busy, and whose next is handed when the running task suspends or finishes; between the two nothing of the strand runs anywhere. A task on a strand that waits leaves the strand to the next task and comes back through the strand's queue when it is woken, behind whoever is queued by then: the strand is held between two suspensions of a task and never across one, so a task that reads a structure, awaits, and writes it does not find it as it left it (a [mutex](mutex.md) is what holds across a wait).

The executor's thread parks when the queue is empty, on the queue's own word, and a push wakes it, after the spin of `config::worker_spin_microseconds` every worker does before it sleeps, so that a task that hops to the main thread and back does not pay two trips through the kernel. What a hop costs, measured once on an M-series laptop (`bench_async`): a round trip `co_await on(ex)` from a worker to an executor and `co_await on_workers()` back, about 640 ns; a round trip `co_await on_workers()` and `co_await on(s)` between the pool and a strand about 85 ns; a `yield` on an executor about 45 ns, against 26 ns for a `yield` on a worker (a ring). The queue of an executor or a strand is a list linked through the frames themselves, so a push allocates nothing: an exchange of the tail and a store of a link.

## Rules

- An executor is run by one thread at a time (`run`, `run_until`, `run` of a task, `poll`); debug builds assert on a second. Which thread is the program's choice: the main thread for a UI, any thread for a library that wants one.
- `stop()` stops the loop, not the tasks: `run()` returns after the frame it is resuming, the tasks stay queued, suspended, and the next `run()` or `poll()` resumes them; a `stop()` with no run in progress makes the next `run()` return at once. From any thread.
- An executor destroyed leaves its tasks suspended for good: the queue is a managed object every frame on it holds through its header, so nothing dangles, a wake of such a task lands on a queue nobody runs, and the frames are the collector's once nothing else holds them, their destructors never run, as with a task the scheduler was stopped under ([scheduler](scheduler.md)). A program that wants its tasks finished runs the executor until they are.
- `run_until(t)` and `run(t)` await the task through a task of the executor: nobody else `co_await`s it meanwhile. `run(t)` returns `t.result()`; a `stop()` before `t` ends leaves nothing to return (debug builds assert).
- A task on a worker waits with `co_await`, and so does a task on an executor: a blocking call there (`ch.receive().wait()`, `t.wait()`) blocks the executor's thread and every task queued on it.
- The executor and the strand live anywhere (a local of `main`, a global, a member): each holds its queue through a root. Neither is copyable.
- A task on a strand that never suspends holds the strand, and its worker, until it does; a task on an executor holds the executor's thread the same way. Cooperative, as every task is.

## Members

### executor

```cpp
executor();

void run();                                    // the calling thread's loop, until stop()
template<class T> T run(async::task<T> t);            // the loop until t is done, and t.result(); starts t here if nobody has
void run(async::task<> t);
template<class T> void run_until(async::task<T>& t);  // the loop until t is done (or stop()); starts t here if nobody has
size_t poll();                                 // one pass: the frames queued at the call run, their number returned
void stop();                                   // run() returns; the tasks stay queued
bool running() const noexcept;                 // a run() or a poll() in progress
template<class T> [[nodiscard]] async::task<T> async::spawn(async::task<T> t);   // started on this executor
template<class T> void async::go(async::task<T> t);          // spawn and detach
template<class F> [[nodiscard]] auto async::spawn(F f);   // a coroutine function with captures, uncalled (scheduler.md: spawn)
template<class F> void async::go(F f);
```

`poll()` runs what was queued when it was called, each frame to its next suspension; a frame queued meanwhile, a task that yielded included, waits for the next call, so a loop that calls `poll()` once per frame of its own is never held by a task that keeps yielding.

```cpp
async::executor ui;
auto t = ui.spawn(refresh(widgets));      // queued; runs when the thread runs the executor
while (window_open()) {
    handle_events();
    ui.poll();                            // the tasks queued since the last frame, to their next wait
    draw();
}
```

### strand

```cpp
strand();

template<class T> [[nodiscard]] async::task<T> async::spawn(async::task<T> t);   // started on this strand, run by a worker in its turn
template<class T> void async::go(async::task<T> t);
template<class F> [[nodiscard]] auto async::spawn(F f);   // a coroutine function with captures, uncalled (scheduler.md: spawn)
template<class F> void async::go(F f);
bool busy() const noexcept;                    // a task of the strand runs or is queued
```

```cpp
async::strand owner;                       // the tasks of one document
async::go(load(doc), owner);
async::go(index(doc), owner);              // after load's first wait at the earliest, never at the same time
```

### on

```cpp
explicit on(async::executor& ex) noexcept;
explicit on(async::strand& s) noexcept;
bool await_ready() const noexcept;
template<class P> bool await_suspend(std::coroutine_handle<P>);
void await_resume() const noexcept;
```

`co_await sgcl::async::on(ex)`: the task goes on on the executor from the next line, and what it awaits from then on wakes it there; at once, without a hop, when it is there already. From a strand to another strand, from an executor to a strand, from the pool to either.

### on_workers

```cpp
bool await_ready() const noexcept;
template<class P> bool await_suspend(std::coroutine_handle<P>);
void await_resume() const noexcept;
```

`co_await sgcl::async::on_workers()`: the task goes on on the pool, from the next line; at once when it is on a worker with no executor.

```cpp
async::task<> on_click(async::executor& ui) {   // a handler on the UI thread
    co_await on_workers();              // the heavy part on the pool
    auto image = co_await decode(file);       // where the awaiter is: the pool
    co_await on(ui);                    // the widget on the UI thread
    show(image);
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <thread>

using namespace sgcl;

// A program that is one task on the main thread, the way a program with
// a UI is: the widgets are touched on the main thread only, the work
// runs on the pool, and a strand serializes the tasks that share a
// counter. Every wait of the main-thread task, a task it awaits, a
// sleep, brings it back to the main thread.
using namespace std::chrono_literals;

static std::thread::id main_thread;

static bool on_main() {
    return std::this_thread::get_id() == main_thread;
}

async::task<long> compute(int n) {                // an awaited task runs where its awaiter runs
    long sum = 0;
    for (int i : range(n)) {
        sum += i;
    }
    co_return sum;
}

async::task<> increment(int& counter, int n) {    // on a strand: a plain int, no lock
    for (int i : range(n)) {
        ++counter;
        if (i % 100 == 99) {
            co_await async::yield();              // leaves the strand to the next task, comes back in its turn
        }
    }
}

async::task<int> program(async::executor& main, async::strand& serial) {
    std::cout << std::boolalpha;
    std::cout << "starts on the main thread: " << on_main() << "\n";
    co_await async::on_workers();
    std::cout << "computes on a worker: " << async::scheduler::on_worker() << "\n";
    long sum = co_await compute(1000);
    co_await async::on(main);
    std::cout << "back on the main thread: " << on_main() << ", the sum " << sum << "\n";
    co_await async::sleep(1ms);                   // the timer thread wakes it: on the main thread
    std::cout << "after a sleep, on the main thread: " << on_main() << "\n";
    int counter = 0;
    vector<async::task<>> tasks;
    for (int t : range(3)) {
        (void)t;
        tasks.push_back(serial.spawn(increment(counter, 1000)));
    }
    for (auto& t : tasks) {
        co_await t;                              // the end of a task on the strand: resumed on the main thread
    }
    std::cout << "the counter, three tasks on a strand, no lock: " << counter << ", on the main thread: " << on_main() << "\n";
    co_return counter == 3000 ? 0 : 1;
}

int main() {
    main_thread = std::this_thread::get_id();
    async::executor main;
    async::strand serial;
    int code = main.run(program(main, serial));         // the program is one task on this thread
    async::scheduler::stop();
    return code;
}
```

The output:

```
starts on the main thread: true
computes on a worker: true
back on the main thread: true, the sum 499500
after a sleep, on the main thread: true
the counter, three tasks on a strand, no lock: 3000, on the main thread: true
```

## See also

- [scheduler](scheduler.md): the pool of workers, `spawn`, `yield`; [task_local](task_local.md): a value a task and its children see, inherited the way the executor is remembered
- [coroutine](coroutine.md): `task`, the frame's header; [managed_frame](../core/coroutine.md): the managed frame under it; [mutex](mutex.md): what must hold across a wait
- [README: Coroutines](README.md#coroutines)
- `tests/async/executor.cpp`: every behaviour above, checked.
