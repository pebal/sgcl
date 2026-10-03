[sgcl](../README.md) › [async](README.md)

# sgcl::async::task_group

```cpp
#include "sgcl/async/task_group.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class task_group;
}
```

`sgcl::async::task_group` is structured concurrency, the way Go's `errgroup` and Kotlin's `coroutineScope` have it,
and Java's `StructuredTaskScope`: a scope that owns the tasks it starts. A group is made under a
[stop_token](stop_token.md), `async::task_group g(token)`, and its own [stop_source](stop_source.md) is a child of
the token's, stopped with it; [go](task_group/go.md) starts a child on the scheduler and counts it, and the wait for
the group is the wait for every child, the three ways the module waits: `g.wait()` blocks a thread, `co_await g`
suspends a task, `g.on_done(f)` is a case of a [select](select.md).

The first child that throws requests the stop of the group, which the others see through
[token](task_group/token.md) and leave; the wait rethrows that first exception once every child has finished, so
nothing of the scope runs on past it and no exception is lost. The children report through their own side effects,
a channel or an object they were given, as `errgroup`'s do; a result that must come back is the business of
[when_all](when_all.md).

The state of a group, its source, the count of the children and the first exception, is a managed object that every
child holds through its frame, so the group object itself may go while children run: its destructor requests the
stop of the scope and lets the children finish on their own, their frames the collector's once they are done, the
way a `coroutineScope` cancelled by its parent ends. That is the fallback, not the way to use it: a scope is waited
for, as `errgroup.Wait` is called, and a child stopped this way still refers to whatever the caller gave it, a
channel on the caller's stack, say, which must outlive it. Under the group, a [wait_group](wait_group.md) counts the
children, a [stop_source](stop_source.md) stops them, and one word claims the first exception.

## Rules

- A group is an object, not a handle: it lives where a `tracked_ptr` may, on a stack or inside a managed object
  ([The rules](../core/README.md#the-rules), 1), and is neither copied nor moved.
- A child is a task nobody spawned, or one spawned already; `go` starts it and the group owns it from then on: the
  `task` object is consumed, its result, if any, dropped, its exception the group's. A child inherits the
  [task-locals](task_local.md) and the [executor](executor.md) of the task that starts it, as a task started by a
  task does.
- A child stops itself when it sees the group's token: a task cannot be stopped from outside
  ([README: The rules](README.md#the-rules), 3).
- The first exception is the one rethrown, by every wait that follows: a second wait rethrows it again, as
  `errgroup.Wait` returns its error again; the others are dropped. A stop is not an error: a group whose children
  were stopped by [request_stop](task_group/request_stop.md) or by the parent waits without throwing.
- The wait returns only when every child has finished, exception or not. `g.wait()` is a thread's: a task on a
  worker writes `co_await g` ([README: The rules](README.md#the-rules), 1); debug builds assert.
- A group that ends with children running stops them and lets them go. Its destructor is noexcept, as every
  destructor is, and throws nothing: a child whose wake would have to start the scheduler's workers and cannot is
  queued all the same, every child waiting on the token is woken, and they run when the workers next start.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](task_group/task_group.md) | constructs a scope under a token |
| `(destructor)` | stops the children still running and lets them go; a group waited for has none |

#### Operations

| Function | Description |
|---|---|
| [go](task_group/go.md) | starts a child on the scheduler and counts it |
| [request_stop](task_group/request_stop.md) | requests the stop of the whole scope |

#### Observers

| Function | Description |
|---|---|
| [token](task_group/token.md) | the token the children are given |
| [stop_requested](task_group/stop_requested.md) | checks whether the stop of the scope has been requested |
| [count](task_group/count.md) | the children not yet finished |

#### Waiting

| Function | Description |
|---|---|
| [wait, operator co_await](task_group/wait.md) | waits for every child, then rethrows the first exception |
| [on_done](task_group/on_done.md) | a case of a select: a call once every child has finished |

## Complexity

A child costs its own frame and one more, the runner's: a small task that awaits the child, records its exception
and counts it off. The runner is run on the calling thread to its first suspension, which puts the child on the
scheduler, and let go of, so that the child's end resumes the runner as its continuation: two frames and two turns
of the scheduler per child, what `co_await` of a spawned task costs.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// Eight workers in one scope: seven serve until told to stop, one fails. The
// failure stops the scope, the others leave when their token says so, and the
// wait gives the exception once every worker has finished.
async::task<int> worker(int id, async::stop_token tok, atomic<int>& left) {
    if (id == 3) {
        co_await async::yield();
        throw runtime_error("worker 3 failed");
    }
    co_await tok.stopped();  // the work: here, waiting for the stop
    ++left;
    co_return id;
}

int main() {
    atomic<int> left = 0;
    async::task_group g;
    for (int id : range(8)) {
        g.go(worker(id, g.token(), left));  // the result is dropped: the group reports exceptions
    }
    try {
        g.wait();
    } catch (const std::exception& e) {
        println("{}, {} left on the stop, {} running", e.what(), left.load(), g.count());
    }
}
```

Output:

```text
worker 3 failed, 7 left on the stop, 0 running
```

## See also

- [stop_token](stop_token.md): what the children see
- [when_all](when_all.md), [when_any](when_any.md): every result back, a race of tasks
- [wait_group](wait_group.md): the count under it
- [with_timeout](with_timeout.md): a deadline on one task
- [go](go.md): a task started with nobody to wait for it
