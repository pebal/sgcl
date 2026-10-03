[sgcl](../README.md) › [async](README.md)

# sgcl::async::spawn

```cpp
#include "sgcl/async/coroutine.h"   // or "sgcl/async.h"
#include "sgcl/async/executor.h"    // (4–5), or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    [[nodiscard]] task<T> spawn(task<T> t);                  // (1)
    template<class F>
    [[nodiscard]] auto spawn(operation<F> op);               // (2)
    template<class F>
    [[nodiscard]] auto spawn(F f);                           // (3)
    template<class T, class Executor>
    [[nodiscard]] task<T> spawn(task<T> t, Executor& ex);    // (4)
    template<class F, class Executor>
    [[nodiscard]] auto spawn(F f, Executor& ex);             // (5)
}
```

Starts a task and returns its handle, for the result: `auto t = async::spawn(f());`, then `t.wait()` on a thread or
`co_await t` in a task. It returns at once; a worker of the [scheduler](scheduler.md) runs the task to its next
suspension, and whatever the task waits for there makes it ready again. Go's `go f()` with a handle kept;
[go](go.md) is the start whose handle nobody keeps.

1. Puts the task on the scheduler's queue of the ready, as its member [spawn](task/spawn.md) does, and gives it back.
2. Runs the [operation](operation.md) `op` concurrently, as a task of its own: `async::spawn(ch.receive())` is a
   task whose result is what `co_await ch.receive()` gives.
3. Starts the task of the coroutine function `f`, passed uncalled: a lambda with captures,
   `async::spawn([x]() -> async::task<int> { ... })`. Takes part only when `f()` returns a task.
4. Starts the task on `ex`, an [executor](executor.md) or a [strand](strand.md): queued there, and run by the thread
   that runs the executor, or by a worker in the strand's turn; `ex.spawn(t)`.
5. (3) on `ex`.

A lambda's captures are fields of the closure object, and a coroutine's frame keeps the closure by `this`, not by
copy (only the parameters of a coroutine are copied into its frame), so the task of a called lambda,
`async::spawn([x]() -> async::task<int> { ... }())`, runs on a closure that died at the end of that statement and
reads freed stack (CppCoreGuidelines CP.51; AddressSanitizer reports a stack-use-after-scope). Passed uncalled (3, 5),
the closure is copied into the frame of a task of its own, which lives as long as the task, and the captures with
it: a `tracked_ptr` captured is a root of the task, as a local would be. A lambda without captures, or a named
coroutine with parameters, may be called and its task passed (1, 4).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to start: one nobody started yet |
| `op` | the operation to run as a task |
| `f` | the coroutine function, called once with no arguments, inside the task |
| `ex` | the executor or the strand to start the task on |

## Return value

- (1), (4) `t`, started.
- (2) A `task` of what `co_await op` gives (a `task<optional<int>>` for the receive of a `channel<int>`), started.
- (3), (5) A task of the type `f()` returns, started; its frame holds the closure.

## Complexity

- (1) Constant: a push on a queue of the scheduler, and the wake of a sleeping worker when none is looking for work.
- (2), (3) The same, plus the frame of the task that carries out the operation or holds the closure.
- (4), (5) A push on the queue of `ex`, plus the frame of (5).

## Exceptions

- (1)–(3) `std::system_error` when the push starts the scheduler (the first start, or the first after a stop) and a
  worker's thread cannot be started; (3) also what the move constructor of `F` throws.
- (4)–(5) What the `spawn` of `ex` throws ([executor](executor.md), [strand](strand.md)).

## Notes

The result is `[[nodiscard]]`: a task whose handle nobody keeps is started with [go](go.md), which says so. A
started task whose object is dropped all the same runs on to its end ([detach](task/detach.md)). A task is started
once: `t` may not have been started already by a spawn, a wait or a `resume` (debug builds assert it). The task takes
the task-locals of the task the calling thread runs, if any ([task_local](task_local.md)).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

async::task<int> square(int x) {
    co_return x * x;
}

int main() {
    auto a = async::spawn(square(6));

    async::channel<int> ch;
    auto r = async::spawn(ch.receive());  // an async::task<optional<int>>
    (void)ch.send(7).wait();

    int n = 3;
    tracked_ptr node = make_tracked<Node>(10);
    auto c = async::spawn([n, node]() -> async::task<int> {  // the closure lives in the task's frame
        co_await async::yield();
        co_return node->value * n;
    });
    node = nullptr;  // the task's frame keeps the node

    println("{} {} {}", a.wait(), *r.wait(), c.wait());
}
```

Output:

```text
36 7 30
```

On an executor run by the calling thread, and on a strand, whose tasks run on the workers one at a time:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<bool> runs_on(thread::id id) {
    co_return this_thread::get_id() == id;
}

int main() {
    async::executor here;
    auto t = async::spawn(runs_on(this_thread::get_id()), here);
    println("{}", here.run(std::move(t)));  // this thread runs the executor until t is done

    async::strand serial;
    int count = 0;  // touched by one task of the strand at a time
    vector<async::task<>> tasks;
    for (int i : range(100)) {
        tasks.push_back(async::spawn([&count]() -> async::task<> {
            ++count;
            co_return;
        }, serial));
    }
    for (auto& task : tasks) {
        task.wait();
    }
    println("{}", count);
}
```

Output:

```text
true
100
```

## See also

- [go](go.md): a start whose handle nobody keeps
- [task](task.md): what is started; [spawn](task/spawn.md): the member
- [when_all](when_all.md), [when_any](when_any.md): every result of several tasks, the first to finish
- [executor](executor.md), [strand](strand.md): where else a task runs
- [spawn_blocking](spawn_blocking.md): a blocking call on a pool of threads apart from the workers
