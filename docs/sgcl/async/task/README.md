[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::task\<T\>

```cpp
#include "sgcl/async/coroutine.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T = void>
    class task;

    template<>
    class task<void>;
}
```

`sgcl::async::task<T>` is a coroutine that produces one value and runs on the [scheduler](../scheduler/README.md): a
coroutine function that returns a `task<T>` and `co_return`s a `T`. [spawn](../spawn.md) puts the task on the queue of
the ready and returns at once, and a worker runs it to its next suspension; whatever it waits for there (a
[channel](../channel/README.md), another task, a timer, [yield](../yield.md)) makes it ready again. A thread waits for its value
with [wait](wait.md), a task with `co_await t`, which suspends the awaiting task until this one is done, with no
thread held meanwhile; either rethrows what the task threw. A task may also be driven by hand, without the
scheduler, one step per [resume](resume.md). The value type is always spelled, `async::task<int>`, and
`async::task<>` is the task of a coroutine that returns nothing.

The frame is a managed one ([managed_frame](../../core/managed_frame.md)): the promise derives from `managed_frame`, so
the parameters, locals, temporaries and promise members of the coroutine are roots while the frame is held, and a
`tracked_ptr` kept across a `co_await` keeps its object. The task object is a [frame_ptr](../../core/frame_ptr/README.md) to
that frame. The four words the core keeps in front of every managed frame hold, for a task, the executor it runs
on, its task-locals and the link of an executor's queue ([executor](../executor/README.md), [task_local](../task_local/README.md)).

In the terms of Go: `spawn(f())` is `go f()` with a handle kept for the result, [go](../go.md) is `go f()` itself, a
task is a goroutine and the workers are Go's P's. What differs: a task is a stackless C++ coroutine, so only the body
of a coroutine can suspend and a function it calls cannot, and it is cooperative, so a task that computes for a
second holds its worker for a second. A task whose handle is dropped is not cancelled; it runs on to its end, as a
goroutine does, and stops early only through a [stop_token](../stop_token/README.md) it looks at itself.

## Rules

- The task is lazy: nothing of the body runs until the first of [spawn](spawn.md), [wait](wait.md),
  `co_await`, [result](result.md) or [resume](resume.md). `spawn` and `wait` put it on the pool of workers,
  `co_await` where the awaiting task runs (its executor, its strand), as a call would; it takes the task-locals of
  the task that starts it. A task resumed by hand on its first run takes the resumer's executor and task-locals.
- A waiting task is nowhere: a frame on the managed heap and a word on the list of what it waits for, which holds the
  frame for the length of the wait, so a task nobody holds may wait.
- A task is awaited by one coroutine at a time (debug builds assert it); the same coroutine may await it again
  after. A task that ended holds nothing of its awaiter.
- [wait](wait.md), [result](result.md) and [done](done.md) may be called from any thread; `wait` and
  a `result` that has to wait block the thread, so a task on a worker `co_await`s instead (debug builds assert it).
- What the coroutine `co_return`s is kept in the frame for `result()`; what it throws is kept there too, and
  `result()`, `wait()` and `co_await` rethrow it, every time. An exception of a task let go of, which nobody reads,
  goes to [on_unhandled](../on_unhandled.md)'s handler.
- A task let go of — its object dropped, assigned over or [detached](detach.md) — runs on to its end if it
  started, wherever it waits, and destroys its frame itself then; one that never started is destroyed with its
  frame. Letting go is not cancelling: a task that is to end early is given a [stop_token](../stop_token/README.md).
- Move-only. The object is two words, a `frame_ptr` that holds the frame by a root, so it lives anywhere: on a
  stack, in a managed object, in a `std` container (a `sgcl::vector<async::task<T>>` holds many tasks).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the value the coroutine `co_return`s, moved into the promise. `void`, the default, for a coroutine that returns nothing: `task<void>` is a [specialization](#specializations). |

## Member types

| Type | Definition |
|---|---|
| `promise_type` | the promise of the coroutine, derived from [managed_frame](../../core/managed_frame.md): `initial_suspend` suspends (the task is lazy), `return_value` moves the value into an `optional<T>`, `unhandled_exception` keeps the exception; the final suspension marks the task done, wakes a thread in `wait()` and hands the awaiting coroutine to the scheduler. A program does not call it. |
| `awaiter` | what `co_await t` uses ([wait, operator co_await](wait.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](task.md) | constructs an empty task, or takes another's over |
| `(destructor)` | lets go of the task: one that started runs on to its end, one that never started is destroyed |
| [operator=](operator_assign.md) | lets go of the task held and takes another's over |

#### Observers

| Function | Description |
|---|---|
| [done](done.md) | checks whether the coroutine has ended |

#### Getting the result

| Function | Description |
|---|---|
| [wait, operator co_await](wait.md) | waits for the task and gives its value |
| [result](result.md) | the value of a task that is done, or what it threw |

#### Operations

| Function | Description |
|---|---|
| [spawn](spawn.md) | puts the task on the scheduler's queue |
| [resume](resume.md) | runs the coroutine by hand, on the calling thread, to its next suspension |
| [detach](detach.md) | lets go of the task, which runs on to its end |
| [destroy](destroy.md) | destroys the coroutine of a task that never ran or is done |

## Specializations

`task<void>` is the task of a coroutine that returns nothing: its promise has `return_void` and no value, `wait()`
and `result()` return `void`, and `co_await t` gives nothing; each rethrows what the coroutine threw, as for
`task<T>`. The other members are the same.

## Complexity

The frame is allocated once, as a managed buffer, when the coroutine function is called. A start is one push on a
queue of the scheduler. The end of a task is one exchange on a word of the promise, a notify only when a thread
waits in `wait()`, and a push of the awaiting coroutine; a `co_await` of a task that is done does not suspend.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
    tracked_ptr<Node> next;
};

// The parameter is a copy in the managed frame: it keeps the list while the task waits
async::task<int> sum(tracked_ptr<Node> head) {
    int s = 0;
    for (auto n = head; n; n = n->next) {
        s += n->value;
        co_await async::yield();  // to the back of the queue; the frame keeps n
    }
    co_return s;
}

async::task<int> twice(tracked_ptr<Node> head) {
    auto a = async::spawn(sum(head));
    auto b = async::spawn(sum(head));
    co_return co_await a + co_await b;
}

int main() {
    tracked_ptr<Node> head;
    for (int i : range(1, 5)) {
        head = make_tracked<Node>(i, head);
    }
    auto t = twice(head);
    head = nullptr;  // the frames hold the list now
    println("{}", t.wait());
}
```

Output:

```text
20
```

## See also

- [spawn](../spawn.md), [go](../go.md): start a task, keeping its handle or not
- [when_all](../when_all.md), [when_any](../when_any.md): every result of several tasks, the first of them to finish
- [generator](../generator/README.md): a coroutine that yields many values and may wait between them
- [task_group](../task_group/README.md), [with_timeout](../with_timeout.md): a scope of tasks, a deadline on one
- [run](../run.md): the task of a program, waited for by `main`
- [on_unhandled](../on_unhandled.md): what becomes of an exception nobody reads
- [managed_frame](../../core/managed_frame.md), [frame_ptr](../../core/frame_ptr/README.md): the managed frame under a task
- [README: Coroutines](../README.md#coroutines)
