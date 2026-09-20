# sgcl::task_group

```cpp
#include "sgcl/async/task_group.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class task_group;   // a scope that owns the tasks it spawns: waited for as one, stopped as one, the first exception rethrown
}
```

Structured concurrency, the way Go's `errgroup` and Kotlin's `coroutineScope` have it: a scope that owns the tasks it spawns. A group is made under a [stop_token](stop_token.md) (`task_group g(token)`: the group's own `stop_source` is a child of the token's, stopped with it), `g.spawn(t)` starts a child on the scheduler and counts it, and the wait for the group is the wait for every child, the three ways everything in this module is waited for: `g.wait()` blocks a thread, `co_await g.async_wait()` suspends a task, `g.on_done(f)` is a case of a [select](select.md). The first child that throws requests the stop of the group, which the others see through `g.token()` and leave; the wait rethrows that first exception once every child has finished, so nothing of the scope runs on past it and no exception is lost. The children report through their own side effects (a channel, an object they were given), as `errgroup`'s do; a result that must come back is [when_all](when.md)'s business.

The state of a group (its source, the count of the children, the first exception) is a managed object that every child holds through its frame, so the group object itself may go while children run: its destructor requests the stop of the scope and lets the children finish on their own, their frames the collector's once they are done, the way a `coroutineScope` cancelled by its parent ends. That is the fallback, not the way to use it: a scope is waited for, as `errgroup.Wait` is called, and a child stopped this way still refers to whatever the caller gave it (a channel on the caller's stack, say), which must outlive it. Under the group: a [wait_group](wait_group.md) counts the children, a `stop_source` stops them, one word claims the first exception.

What a spawn costs: the child's frame and one more, the runner's, a small task that awaits the child, records its exception and counts it off. The runner is run on the calling thread to its first suspension (which puts the child on the scheduler) and let go of, so that the child's finish resumes the runner as its continuation: two frames and two turns of the scheduler per child, what `co_await` of a spawned task costs. Measured once (Apple M-series, release, groups of 1000 children that return at once, 2000 rounds): `spawn` and `wait` together cost about 1150 ns per child from a thread, where every child goes through the global queue, and about 740 ns per child for a group made inside a task, where the children go on the worker's own ring; a `when_all` over a vector of 1000 tasks spawned from a thread costs about 1000 ns per task, so the group's own share, the runner's frame and the count, is about 150 ns.

## Rules

- A child is a task nobody spawned, or one spawned already; `spawn` starts it and the group owns it from then on: the `task` object is consumed, its result, if any, dropped, its exception the group's. A child stops itself when it sees the group's token: a task cannot be stopped from outside. A child inherits the spawning task's [task-locals](task_local.md) and its [executor](executor.md), as a task started by a task does.
- The first exception is the one rethrown, by every wait that follows (a second `wait()` rethrows it again, as `errgroup.Wait` returns its error again); the others are dropped. A stop is not an error: a group whose children were stopped by `request_stop()` or the parent waits without throwing.
- `wait()` returns only when every child has finished, exception or not; `wait()` is not called on a worker (it would block it): a task `co_await`s `async_wait()`.
- `on_done(f)` is served when the count reaches zero; it is a case for a group that spawned at least one child (a group that never did has no round to close). The exception, if any, is rethrown by the `wait()` that follows, which returns at once.
- A group lives where a `tracked_ptr` may: on a stack or inside a managed object, and is neither copied nor moved. A group that ends with children running stops them and lets them go (above).

## Members

```cpp
explicit task_group(const stop_token& parent = stop_token());   // a scope under the token; an empty token: a scope on its own
~task_group();                                                  // children still running: stopped and let go of
template<class T> void spawn(task<T> t);                        // a child: started on the scheduler, counted
template<class F> void spawn(F f);                              // a coroutine function with captures, uncalled (scheduler.md: spawn)
stop_token token() const noexcept;                              // what the children are given: stopped by the first exception, request_stop, the parent, the group's end
void request_stop();                                            // the stop of the whole scope, by hand
bool stop_requested() const noexcept;
size_t count() const noexcept;                                  // the children not yet finished
void wait();                                                    // a thread: every child finished, then the first exception rethrown
task<> async_wait();                                            // a task: co_await g.async_wait(), the same
template<class F> auto on_done(F f);                            // a case of a select: f() when every child has finished
```

```cpp
sgcl::task<> fetch(sgcl::string url, sgcl::channel<sgcl::string>& out, sgcl::stop_token tok) {
    co_await sgcl::async_select(
        out.on_send(url + ": ok"),                                 // the work, here a send
        tok.on_stop([] {})                                         // or the stop: another child failed, or the caller gave up
    );
}

sgcl::task<> fetch_all(sgcl::vector<sgcl::string> urls, sgcl::channel<sgcl::string>& out, sgcl::stop_token tok) {
    sgcl::task_group g(tok);                                       // a scope under the caller's token
    for (auto& url : urls) {
        g.spawn(fetch(url, out, g.token()));                       // every child gets the group's token
    }
    co_await g.async_wait();                                       // every child finished; the first exception rethrown
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// Eight workers on one scope: seven serve requests until told to stop,
// one fails after a while. The failure stops the scope, the others leave
// when their token says so, and the wait gives the exception once every
// worker has finished: nothing runs on past the scope.
sgcl::task<int> worker(int id, sgcl::stop_token tok, sgcl::atomic<int>& left) {
    if (id == 3) {
        co_await sgcl::sleep(10ms);
        throw std::runtime_error("worker 3 failed");
    }
    co_await tok.stopped();                                   // the work: here, waiting for the stop
    ++left;
    co_return id;
}

int main() {
    sgcl::atomic<int> left = {0};
    sgcl::task_group g;
    for (int id : sgcl::range(8)) {
        g.spawn(worker(id, g.token(), left));                 // a result is dropped: the group reports exceptions only
    }
    try {
        g.wait();
    } catch (const std::exception& e) {
        std::cout << e.what() << ", " << left.load() << " left on the stop, " << g.count() << " running\n";
    }
    return g.stop_requested() && left.load() == 7 ? 0 : 1;
}
```

The output:

```
worker 3 failed, 7 left on the stop, 0 running
```

## See also

- [stop_token](stop_token.md): what the children see; [when](when.md): every result back, a race of tasks; [wait_group](wait_group.md): the wait group under it; [timeout](timeout.md): a deadline on one task
- `tests/async/task_group.cpp`: every behaviour above, checked.
