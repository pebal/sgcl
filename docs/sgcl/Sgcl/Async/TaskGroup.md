# TaskGroup

```cpp
#include "sgcl/Sgcl/Async/TaskGroup.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class TaskGroup;   // a scope that owns the tasks it spawns: waited for as one, stopped as one, the first exception rethrown
}
```

The same in the `sgcl` interface: [task_group](../../async/task_group.md).

Structured concurrency, the way Go's `errgroup` and Kotlin's `coroutineScope` have it: a scope that owns the tasks it spawns. A group is made under a [StopToken](StopToken.md) (`TaskGroup g(token)`: the group's own `StopSource` is a child of the token's, stopped with it), `g.Spawn(t)` starts a child on the scheduler and counts it, and the wait for the group is the wait for every child, the three ways everything in this module is waited for: `g.Wait()` blocks a thread, `co_await g.AsyncWait()` suspends a task, `g.OnDone(f)` is a case of a [Select](Select.md). The first child that throws requests the stop of the group, which the others see through `g.Token()` and leave; the wait rethrows that first exception once every child has finished, so nothing of the scope runs on past it and no exception is lost. The children report through their own side effects (a channel, an object they were given), as `errgroup`'s do; a result that must come back is [WhenAll](When.md)'s business.

The state of a group (its source, the count of the children, the first exception) is a managed object that every child holds through its frame, so the group object itself may go while children run: its destructor requests the stop of the scope and lets the children finish on their own, their frames the collector's once they are done, the way a `coroutineScope` cancelled by its parent ends. That is the fallback, not the way to use it: a scope is waited for, as `errgroup.Wait` is called, and a child stopped this way still refers to whatever the caller gave it, which must outlive it. Under the group: a [WaitGroup](WaitGroup.md) counts the children, a `StopSource` stops them, one word claims the first exception; a spawn costs the child's frame and a runner's, two turns of the scheduler per child (the numbers on the [sgcl page](../../async/task_group.md)).

## Rules

- A child is a task nobody spawned, or one spawned already; `Spawn` starts it and the group owns it from then on: the `Task` object is consumed, its result, if any, dropped, its exception the group's. A child stops itself when it sees the group's token: a task cannot be stopped from outside. A child inherits the spawning task's [task-locals](TaskLocal.md) and its [executor](Executor.md), as a task started by a task does.
- The first exception is the one rethrown, by every wait that follows; the others are dropped. A stop is not an error: a group whose children were stopped by `RequestStop()` or the parent waits without throwing.
- `Wait()` returns only when every child has finished, exception or not; `Wait()` is not called on a worker: a task `co_await`s `AsyncWait()`.
- `OnDone(f)` is served when the count reaches zero; it is a case for a group that spawned at least one child. The exception, if any, is rethrown by the `Wait()` that follows, which returns at once.
- A group lives where a `Ptr` may: on a stack or inside a managed object, and is neither copied nor moved. A group that ends with children running stops them and lets them go (above).

## Members

```cpp
explicit TaskGroup(const StopToken& parent = StopToken());   // a scope under the token; an empty token: a scope on its own
~TaskGroup();                                                // children still running: stopped and let go of
template<class T> void Spawn(Task<T> t);                     // a child: started on the scheduler, counted
template<class F> void Spawn(F f);                           // a coroutine function with captures, uncalled (Scheduler.md: Spawn)
StopToken Token() const noexcept;                            // what the children are given: stopped by the first exception, RequestStop, the parent, the group's end
void RequestStop();                                          // the stop of the whole scope, by hand
bool IsStopRequested() const noexcept;
size_t Count() const noexcept;                               // the children not yet finished
void Wait();                                                 // a thread: every child finished, then the first exception rethrown
Task<> AsyncWait();                                          // a task: co_await g.AsyncWait(), the same
template<class F> auto OnDone(F f);                          // a case of a Select: f() when every child has finished
sgcl::task_group& Inner() noexcept;
```

```cpp
Task<> Fetch(String url, Channel<String>& out, StopToken tok) {
    co_await AsyncSelect(
        out.OnSend(url + ": ok"),                                 // the work, here a send
        tok.OnStop([] {})                                         // or the stop: another child failed, or the caller gave up
    );
}

Task<> FetchAll(List<String> urls, Channel<String>& out, StopToken tok) {
    TaskGroup g(tok);                                             // a scope under the caller's token
    for (auto& url : urls) {
        g.Spawn(Fetch(url, out, g.Token()));                      // every child gets the group's token
    }
    co_await g.AsyncWait();                                       // every child finished; the first exception rethrown
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// Eight workers on one scope: seven serve until told to stop, one fails
// after a while. The failure stops the scope, the others leave when
// their token says so, and the wait gives the exception once every
// worker has finished: nothing runs on past the scope.
Task<int> Worker(int id, StopToken tok, Atomic<int>& left) {
    if (id == 3) {
        co_await Sleep(10ms);
        throw std::runtime_error("worker 3 failed");
    }
    co_await tok.Stopped();                                   // the work: here, waiting for the stop
    ++left;
    co_return id;
}

int main() {
    Atomic<int> left = {0};
    TaskGroup g;
    for (int id : Range(8)) {
        g.Spawn(Worker(id, g.Token(), left));                 // a result is dropped: the group reports exceptions only
    }
    try {
        g.Wait();
    } catch (const std::exception& e) {
        std::cout << e.what() << ", " << left.Load() << " left on the stop, " << g.Count() << " running\n";
    }
    return g.IsStopRequested() && left.Load() == 7 ? 0 : 1;
}
```

The output:

```
worker 3 failed, 7 left on the stop, 0 running
```

## See also

- [StopToken](StopToken.md): what the children see; [WhenAll, WhenAny](When.md): every result back, a race of tasks; [WaitGroup](WaitGroup.md): the wait group under it; [Timeout](Timeout.md): a deadline on one task
- `tests/Sgcl/task_group_and_timeout.cpp`: the behaviour above, checked.
