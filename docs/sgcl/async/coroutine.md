# sgcl::async::task, sgcl::async::generator

```cpp
#include "sgcl/sgcl.h"          // or "sgcl/async/coroutine.h"

namespace sgcl {
    template<class T = void> class task;
    template<class T> class generator;   // sgcl/async/generator.h
}
```

`async::task<T>` is a coroutine that produces one value: `async::spawn()` puts it on the scheduler's queue and a worker runs it, `resume()` runs it by hand on the calling thread; a thread waits for it with `wait()`, a coroutine with `co_await task`, which suspends the awaiting coroutine until the task is done, no thread held meanwhile. Its frame is a managed one ([managed_frame, frame_ptr](../core/coroutine.md)): the promise derives from `managed_frame`, and the parameters, locals, temporaries and promise members of the coroutine are roots while the frame is held. The four words the core keeps in front of every managed frame hold, for a task, the executor it runs on, its task-locals and the link of an executor's queue ([executor](executor.md), [task_local](task_local.md)). `async::generator<T>` is a generator that may wait between its values; the one that does not, [`generator<T>`](../core/generator.md), is the core's. The value type is always spelled, `async::task<int>`, `async::generator<int>`.

## Rules

- The parameters, locals, temporaries and promise members of a coroutine whose promise derives from `managed_frame` are roots while the frame is held: a `tracked_ptr`, a container, a `task` held across a suspension all keep what they refer to. A waiting coroutine (on a channel, on a task, on the scheduler's queue) is held by what it waits on, so a detached task's frame lives while it runs. A promise that does not derive from `managed_frame` lives in `operator new` memory together with the rest of the frame, so neither it nor the coroutine's locals or parameters may hold a `tracked_ptr` (rule 1); the collector would not see the pointer, and the object could be collected while the coroutine is suspended.
- A `std::coroutine_handle` keeps nothing alive (rule 3). The handle a `frame_ptr` returns is valid while that `frame_ptr` holds the frame and no longer: once the `frame_ptr` is destroyed, moved from or `destroy()`ed, the frame's memory belongs to the collector. Destroy the coroutine through the `frame_ptr`, never through the handle: the `frame_ptr` would destroy it a second time.
- `wait()` and `done()` of a task may be called from any thread. A task resumed by hand (`resume()`) on its first run takes the resumer's executor and task-locals, as a task started on the scheduler takes its starter's. A task is awaited by one coroutine at a time (an assertion in debug builds); a task that ended holds nothing of its awaiter.

## Members

### task

```cpp
template<class T = void>
class task;
```

A coroutine that produces one value and runs on the [scheduler](scheduler.md) or by hand. `async::spawn()` puts it on the queue of the ready and returns at once (so does the first `wait()` or `co_await` of a task nobody spawned: a task starts with the first of the three); a worker runs it to its next suspension, and the send, the task or the timer it waits for makes it ready again. `resume()` runs it on the calling thread instead, a step at a time, as before the scheduler existed. What it `co_return`s is kept for `result()`: a thread waits for it with `wait()`, a coroutine with `co_await task`, which suspends the awaiting coroutine until the task is done, with no thread held meanwhile; either rethrows what the task threw. `async::task<void>` is the same without a value. A task dropped while it runs is detached (`detach()`): its frame lives while it is queued, waiting or running, and is the collector's once it is done. Move-only, two words (a `frame_ptr<promise_type>`); a default-constructed `task` is empty and `done()`.

#### promise_type

```cpp
struct promise_type : detail::TaskPromiseBase {   // : managed_frame
    std::optional<T> value;               // task<T> only
    std::exception_ptr error;

    async::task get_return_object();
    std::suspend_always initial_suspend() noexcept;
    /* final awaiter */ final_suspend() noexcept;   // marks the task done, wakes a wait() that waits (none waiting: no notify, which was a fetch-add on a shared table per task end), hands the awaiting coroutine to the scheduler
    void return_value(T v);               // task<T>: value.emplace(std::move(v))
    void return_void() noexcept;          // task<void>
    void unhandled_exception() noexcept;  // error = std::current_exception()
};
```

The promise of a `task`. `initial_suspend` is `suspend_always`, so the coroutine body does not run until `async::spawn()` or the first `resume()`; at the final suspension the frame with its result stays until the `task` is destroyed, the task is marked done (one word: running, done, or the handle of the coroutine awaiting it), a thread in `wait()` is woken, and a coroutine that `co_await`ed the task is handed to the scheduler. The value goes into `value`, an exception into `error`; `result()` reads them. A coroutine returning `async::task<T>` may `co_await` a channel, another task, `sgcl::async::yield()`, or `std::suspend_always{}` (the plain "give control back to the resumer", for a task run by hand).

```cpp
async::task<int> count_to(int n) {           // nothing runs yet: the task is lazy
    int i = 0;
    while (i < n) {
        ++i;
        co_await async::yield();           // to the back of the scheduler's queue; the frame keeps i
    }
    co_return i;
}
```

#### Constructor

```cpp
task() noexcept = default;
```

An empty task: `done()` is `true`, `resume()`, `wait()` and `result()` may not be called. A `task` with a coroutine comes from calling a coroutine function that returns one; it is move constructible and move assignable, not copyable.

```cpp
async::task<int> t;                          // empty, to be assigned
t = count_to(3);                          // the coroutine, suspended before its first statement
```

#### spawn

```cpp
async::task& async::spawn();                            // the member
template<class T> async::task<T> async::spawn(async::task<T> t);   // the free function: spawn(f()) for `auto t = spawn(f());`
template<class F> auto async::spawn(async::operation<F> op);    // an operation run as a task of its own: spawn(ch.receive())
```

Puts the task on the scheduler's queue and returns at once; a worker runs it. Once, before the task runs. The free function returns the task, for `auto t = sgcl::async::spawn(f());`. Both are `[[nodiscard]]`: a task object dropped destroys the coroutine, so a task nobody waits for is started with `sgcl::async::go(f())` instead. Given an [operation](README.md#waiting-operations), `spawn` runs it concurrently as a task of its own, whose result is what `co_await` of the operation gives, also `[[nodiscard]]`.

```cpp
auto t = async::spawn(count_to(3));        // running on a worker, or about to
auto r = async::spawn(ch.receive());       // an async::task<optional<int>> for an async::channel<int> ch
```

#### resume

```cpp
void resume();
```

Runs the coroutine on the calling thread to its next suspension or to its end, as a `std::coroutine_handle` would: for a task driven by hand, without the scheduler. Precondition: not empty, not `done()`, not spawned. An exception the coroutine throws does not escape `resume()`: the promise stores it and the task is `done()`; `result()` rethrows it.

```cpp
async::task<int> step(int n) {
    int i = 0;
    while (i < n) {
        ++i;
        co_await std::suspend_always{};   // back to the resumer
    }
    co_return i;
}
async::task<int> t = step(3);
t.resume();                               // i == 1, suspended
t.resume();                               // i == 2
```

#### done

```cpp
bool done() const noexcept;
```

Whether the coroutine has ended (or the task is empty). From any thread.

```cpp
while (!t.done()) {
    this_thread::yield();            // or wait(), which waits without spinning
}
```

#### wait

```cpp
T& wait();                                // task<T>
void wait();                              // task<void>
```

Waits, on the calling thread, until the task is done, and returns `result()` (as `std::this_thread::sync_wait` gives a sender's result). A task nobody started yet is put on the scheduler first: `count_to(3).wait()` is `async::spawn(count_to(3)).wait()`. For a thread; not from a task on a worker, which would block the worker (`co_await` the task there; debug builds assert it).

```cpp
int n = async::spawn(count_to(3)).wait();   // 3, or the exception the coroutine threw
```

#### co_await task

```cpp
awaiter operator co_await() noexcept;    // co_await t: T, or void; rethrows
```

From a coroutine with a managed frame: suspends it until the task is done, with no thread held, and gives the result. A task nobody started yet is put on the scheduler first, so `co_await count_to(3)` runs the task on a worker and waits for it; a task that is done already does not suspend. One coroutine may await a task at a time; the same coroutine may await it again after.

```cpp
async::task<int> twice() {
    int a = co_await async::spawn(count_to(3));
    int b = co_await async::spawn(count_to(4));
    co_return a + b;                      // 7
}
```

#### result

```cpp
T& result();                              // task<T>
void result();                            // task<void>
```

The value the coroutine `co_return`ed, a reference into the frame that is valid while the task holds it; for `async::task<void>`, nothing. If the coroutine ended with an exception, `result()` rethrows it, every time it is called. A task driven by hand with `resume()` is not waited for: its `result()` needs it `done()`, since nobody else will resume it. A task that is not `done()` yet is waited for first, on the calling thread, as `wait()` does (so not from a task on a worker; `co_await` it there). Precondition: not empty.

```cpp
auto t = async::spawn(count_to(3));
t.wait();
int n = t.result();                       // 3, again
```

#### detach

```cpp
void detach() noexcept;
```

Lets go of the task: it runs on, or stays wherever it waits, and destroys its frame when it is done (the locals and parameters with it: a task it awaited, a `root_ptr` it held, released), the memory the collector's from then on; a task detached after it is done is destroyed at once. The task object is empty after. For a task whose result nobody needs; `sgcl::async::go(f())` is a spawn and a detach in one. A task detached before anyone started it never runs: its frame is left to the collector as it is, its locals never destroyed.

```cpp
auto t = async::spawn(log_forever(queue));
t.detach();                               // the same as go(log_forever(queue));
```

#### go

```cpp
template<class T> void async::go(async::task<T> t);
```

Puts the task on the scheduler and lets go of it, Go's `go f()`: `t.spawn().detach()`. For a task nobody waits for; the handle of one somebody does is kept from `sgcl::async::spawn`.

```cpp
async::go(log_forever(queue));
```

#### destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine, running the destructors of its locals and promise, and leaves the task empty; the frame's memory goes to the collector. For a task that never ran or is done, never for one that is queued or waiting (detach it instead). The task's destructor does the same.

```cpp
async::task<int> t = count_to(3);
t.resume();
t.destroy();                              // the frame's locals are gone; t is empty and done()
```

### async::generator

```cpp
#include "sgcl/async/generator.h"   // or "sgcl/sgcl.h"

template<class T>
class generator;

next_op next() noexcept;                  // co_await g.next(): std::optional<T>, nothing at the end; rethrows
bool done() const noexcept;
```

A generator that may wait: a coroutine that `co_yield`s values and `co_await`s between them (a channel, a sleep, a task), consumed from a task with `while (auto v = co_await g.next())`. The consumer and the generator hand control to each other directly, without the scheduler's queue: `next()` resumes the generator on the consumer's worker, a `co_yield` resumes the consumer where the generator is, and while the generator waits for something the consumer waits with it, no thread held by either. The generator runs as part of its consumer: at every `next()` its frame takes the consumer's executor and task-locals, so a wait of its own resumes it where the consumer runs (an [executor](executor.md), a strand) and the functions under it, and the consumer resumed by the yield, see the consumer's [task-locals](task_local.md); a local the generator sets itself lasts until its next yield. Both frames are on the managed heap: the generator's held by the `async::generator` object, the consumer's by the generator's promise while it waits, and a value yielded is held by the promise until the consumer takes it. Move-only; single pass; `next()` past the end gives nothing again; an exception the generator throws comes out of the `next()` that ran into it.

```cpp
async::generator<int> tens(async::channel<int>& in) {
    while (auto v = co_await in.receive()) {    // waits between yields
        co_yield *v * 10;
    }
}

async::task<int> consume(async::channel<int>& in) {
    auto g = tens(in);
    int sum = 0;
    while (auto v = co_await g.next()) {              // nothing once in is closed and drained
        sum += *v;
    }
    co_return sum;
}
```

## Example

```cpp
#include "sgcl/sgcl.h"

#include <coroutine>
#include <iostream>

using namespace sgcl;

struct Node {
    Node(int v, tracked_ptr<Node> n) : value(v), next(n) {}
    int value;
    tracked_ptr<Node> next;
};

// Yields the nodes of a chain it builds as it goes: the local keeps
// the whole chain alive while the generator is suspended
generator<tracked_ptr<Node>> chain(int count) {
    tracked_ptr<Node> last;                         // a local in a managed frame: a root
    for (int i : range(1, count + 1)) {
        tracked_ptr n = make_tracked<Node>(i, last);
        last = n;
        co_yield n;
    }
}

// Sums a chain one node per resume: the parameter copy in the frame keeps the head
async::task<int> sum(tracked_ptr<Node> head) {
    int s = 0;
    for (auto n = head; n; n = n->next) {
        s += n->value;
        co_await std::suspend_always{};                 // back to the caller, the chain stays alive
    }
    co_return s;
}

int main() {
    tracked_ptr<Node> head;
    for (auto& n : chain(4)) {                          // the generator's frame holds the chain
        head = n;                                       // the last one yielded is the head
    }
    collector::force_collect();                     // optional, only to show the point at once: the
                                                        // generator is gone, head keeps the chain
    async::task<int> t = sum(head);
    head = nullptr;                                     // the task's frame is the only root now
    while (!t.done()) {
        t.resume();
        collector::force_collect();                 // optional: the chain survives every cycle
    }
    std::cout << t.result() << '\n';                    // 10
}
```

The output:

```
10
```

## See also

- [managed_frame, frame_ptr](../core/coroutine.md): the managed frame under a task; [generator](../core/generator.md): the core's generator, which does not wait
- [tracked_ptr](../core/tracked_ptr.md), [unique_ptr](../core/unique_ptr.md), [vector](../core/vector.md) (a `sgcl::vector<async::task<T>>` holds many tasks)
- [collector](../core/collector.md), [config](../core/config.md)
- README: [Coroutines](README.md#coroutines), [The rules](../core/README.md#the-rules), [Stack roots](../../garbage_collector/overview.md#stack-roots), [Threads](README.md#threads)
- `tests/async/coroutine.cpp` for the frame as a root: a local, a parameter, a frame in a managed object, a thousand suspended frames in a `sgcl::vector`
