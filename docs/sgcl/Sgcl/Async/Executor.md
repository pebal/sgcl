# Executor, Strand, On, OnWorkers

```cpp
#include "sgcl/Sgcl/Async/Executor.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class Executor;      // a queue of tasks that one thread runs: the thread of the program's choosing
    class Strand;        // an executor with no thread of its own: its tasks run on the workers, one at a time, in order
    class On;            // co_await On(ex): the task goes on on the executor or the strand
    using OnWorkers = ...;   // co_await OnWorkers(): the task goes on on the pool
    template<class T, class E> Task<T> Spawn(Task<T> t, E& ex);   // ex.Spawn(t)
    template<class T, class E> void Go(Task<T> t, E& ex);         // ex.Go(t)
}
```

The same in the `sgcl` interface: [executor, strand, on, on_workers](../../async/executor.md).

The [Scheduler](Scheduler.md) runs a task on whichever worker is free. A platform's UI toolkit (Cocoa, Win32, GTK, the browser) and many a C library demand one thread, usually the main one, for every call into them; an `Executor` is a queue of frames that only the thread running the executor resumes. `ex.Run()` is that thread's loop: what is queued, run to its next suspension; nothing queued, parked; until `Stop()`. `ex.Run(task)` is the loop until the task is done, and the task's result, which makes a program one task on the main thread: `int main() { Executor main; return main.Run(Program()); }`. `ex.Poll()` is one pass over what is queued and a return, for a loop that is not the library's, a `CFRunLoop`, a `GMainLoop`, a game's frame, which calls it when it has a moment. A task moves to an executor with `co_await On(ex)` (Kotlin's `withContext(Dispatchers.Main)`) and back to the pool with `co_await OnWorkers()`; `ex.Spawn(t)` starts a task on it.

A task on an executor stays on it: its frame remembers the executor, and every wake of the frame, whatever woke it, a channel, a timer, a task it awaited, a `Yield`, a `Select`, an event, is routed to the executor's queue. So a task that does `co_await Sleep(1s)` on the main thread is resumed on the main thread, though the timer fired on the timer thread. A task that the task awaits (`co_await F()` of a task nobody started) runs where the awaiter runs, as a call would; a task it starts with `Spawn` or `Go` runs on the pool, one it starts with `ex.Spawn` or `ex.Go` on the executor.

A `Strand` is an executor with no thread of its own (Boost.Asio's strand): its tasks run on the workers, never two at once, in the order they were queued. Serial access to something without a lock. A task on a strand that waits leaves the strand to the next task and comes back through the strand's queue when it is woken: the strand is held between two suspensions of a task and never across one (a [Mutex](Mutex.md) is what holds across a wait).

The costs and the mechanism are the `sgcl` page's: a round trip from a worker to an executor and back about 1 µs, a hop between two strands about 240 ns, a `Yield` on either about 200 ns.

## Rules

- An executor is run by one thread at a time (`Run`, `RunUntil`, `Poll`); debug builds assert on a second.
- `Stop()` stops the loop, not the tasks: `Run()` returns after the frame it is resuming, the tasks stay queued, and the next `Run()` or `Poll()` resumes them; a `Stop()` with no run in progress makes the next `Run()` return at once. From any thread.
- An executor destroyed leaves its tasks suspended for good, their destructors never run, the frames the collector's once nothing else holds them; nothing dangles. A program that wants its tasks finished runs the executor until they are.
- `RunUntil(t)` and `Run(t)` await the task through a task of the executor: nobody else `co_await`s it meanwhile. `Run(t)` returns `t.Result()`; a `Stop()` before `t` ends leaves nothing to return (debug builds assert).
- A task on an executor waits with `co_await`, as one on a worker does: a blocking call there blocks the executor's thread and every task queued on it.
- The executor and the strand live anywhere; neither is copyable.

## Members

### Executor

```cpp
Executor();

void Run();                                    // the calling thread's loop, until Stop()
template<class T> T Run(Task<T> t);            // the loop until t is done, and t.Result(); starts t here if nobody has
void Run(Task<> t);
template<class T> void RunUntil(Task<T>& t);   // the loop until t is done (or Stop()); starts t here if nobody has
size_t Poll();                                 // one pass: the tasks queued at the call run, their number returned
void Stop();                                   // Run() returns; the tasks stay queued
bool IsRunning() const noexcept;               // a Run() or a Poll() in progress
template<class T> [[nodiscard]] Task<T> Spawn(Task<T> t);   // started on this executor
template<class T> void Go(Task<T> t);          // Spawn and Detach
sgcl::executor& Inner() noexcept;
```

```cpp
Executor ui;
auto t = ui.Spawn(Refresh(widgets));      // queued; runs when the thread runs the executor
while (WindowOpen()) {
    HandleEvents();
    ui.Poll();                            // the tasks queued since the last frame, to their next wait
    Draw();
}
```

### Strand

```cpp
Strand();

template<class T> [[nodiscard]] Task<T> Spawn(Task<T> t);   // started on this strand, run by a worker in its turn
template<class T> void Go(Task<T> t);
bool IsBusy() const noexcept;                  // a task of the strand runs or is queued
sgcl::strand& Inner() noexcept;
```

```cpp
Strand owner;                             // the tasks of one document
Go(Load(doc), owner);
Go(Index(doc), owner);                    // after Load's first wait at the earliest, never at the same time
```

### On, OnWorkers

```cpp
explicit On(Executor& ex) noexcept;       // co_await On(ex): the task goes on there; at once when it is there already
explicit On(Strand& s) noexcept;
using OnWorkers = sgcl::on_workers;       // co_await OnWorkers(): the task goes on on the pool
```

```cpp
Task<> OnClick(Executor& ui) {            // a handler on the UI thread
    co_await OnWorkers();                 // the heavy part on the pool
    auto image = co_await Decode(file);   // where the awaiter is: the pool
    co_await On(ui);                      // the widget on the UI thread
    Show(image);
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>
#include <thread>

// A program that is one task on the main thread, the way a program with
// a UI is: the widgets are touched on the main thread only, the work
// runs on the pool, and a strand serializes the tasks that share a
// counter. Every wait of the main-thread task, a task it awaits, a
// sleep, brings it back to the main thread.
using namespace std::chrono_literals;

static std::thread::id mainThread;

static bool OnMain() {
    return std::this_thread::get_id() == mainThread;
}

Task<long> Compute(int n) {                      // an awaited task runs where its awaiter runs
    long sum = 0;
    for (int i : Range(n)) {
        sum += i;
    }
    co_return sum;
}

Task<> Increment(int& counter, int n) {          // on a strand: a plain int, no lock
    for (int i : Range(n)) {
        ++counter;
        if (i % 100 == 99) {
            co_await Yield();                    // leaves the strand to the next task, comes back in its turn
        }
    }
}

Task<int> Program(Executor& main, Strand& serial) {
    std::cout << std::boolalpha;
    std::cout << "starts on the main thread: " << OnMain() << "\n";
    co_await OnWorkers();
    std::cout << "computes on a worker: " << Sgcl::Scheduler::OnWorker() << "\n";
    long sum = co_await Compute(1000);
    co_await On(main);
    std::cout << "back on the main thread: " << OnMain() << ", the sum " << sum << "\n";
    co_await Sleep(1ms);                         // the timer thread wakes it: on the main thread
    std::cout << "after a sleep, on the main thread: " << OnMain() << "\n";
    int counter = 0;
    List<Task<>> tasks;
    for (int t : Range(3)) {
        (void)t;
        tasks.Add(serial.Spawn(Increment(counter, 1000)));
    }
    for (auto& t : tasks) {
        co_await t;                              // the end of a task on the strand: resumed on the main thread
    }
    std::cout << "the counter, three tasks on a strand, no lock: " << counter << ", on the main thread: " << OnMain() << "\n";
    co_return counter == 3000 ? 0 : 1;
}

int main() {
    mainThread = std::this_thread::get_id();
    Executor main;
    Strand serial;
    int code = main.Run(Program(main, serial));  // the program is one task on this thread
    Sgcl::Scheduler::Stop();
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

- [Scheduler](Scheduler.md): the pool of workers, `Spawn`, `Yield`; [TaskLocal](TaskLocal.md): a value a task and its children see
- [Task](Coroutine.md): the tasks, the frames on the managed heap; [Mutex](Mutex.md): what must hold across a wait
- [README: Coroutines](../../async/README.md#coroutines)
- `tests/Sgcl/executor.cpp`: the facade, checked; `tests/async/executor.cpp`: every behaviour.
