[sgcl](../README.md) › [async](README.md)

# sgcl::async::spawn_blocking

```cpp
#include "sgcl/async/blocking.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class F>
    auto spawn_blocking(F f);
}
```

Runs a blocking call off the workers. A worker of the [scheduler](scheduler/README.md) runs every task that is ready, and a
call that blocks it (a file read without the reactor, `getaddrinfo`, a C library, a database driver) takes it from all
of them for as long as the call lasts. `co_await async::spawn_blocking(f)` runs `f` on the
[blocking pool](blocking_pool/README.md) instead, threads apart from the workers and meant to sit in the kernel, and hands
back what `f` returned, or rethrows what it threw: the task holds no thread while the call runs, and the workers go
on with the other tasks. A thread waits for it with `spawn_blocking(f).wait()`.

This is tokio's `spawn_blocking` and Java's `Executors.newCachedThreadPool` under a `co_await`. Go's answer is a
goroutine whose thread the runtime replaces while it is in the call, which C++ coroutines cannot do, so the call goes
to a thread that is nobody's worker.

`f` is moved into a job, a managed object that holds the closure and the [promise](promise/README.md) the result comes back
through, so the closure may capture `tracked_ptr`s by value: they are traced through the job's pointer map. What it
captures by reference must outlive the job, which a task's locals do while the task awaits it; a task that drops the
handle and goes on must not have lent it a reference. The job runs whether or not the handle is kept: a handle
dropped is a job whose result nobody reads, what `f` throws included, and the job is the collector's once it ran. A
call nobody waits for goes through [go_blocking](go_blocking.md), which keeps no handle and gives what `f` throws to
[on_unhandled](on_unhandled.md)'s handler.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the call: a callable with no arguments, run once on a thread of the pool |

## Return value

A [blocking_task](blocking_task/README.md)`<T>`, the handle of the job, `T` what `f` returns (`void` for nothing):
`co_await` it in a task, `wait()` for it on a thread, or give its `on_done` to a [select](select.md).

## Complexity

Constant: the job made, a push on the pool's lock-free queue, and the pool's lock taken once to wake an idle thread
or start one. The round trip, two hand-offs between threads through the kernel, is some microseconds: a call worth
the pool blocks for longer than that, and a call of a few microseconds is cheaper on the worker.

## Exceptions

- The call: what the move constructor of `F` throws, and `std::system_error` when the pool has no thread and cannot
  start one. Then `f` never runs, and nothing of the job is left behind. A pool that has a thread but cannot start
  another throws nothing: the job waits in the queue for a thread to free up, as at the cap.
- Carried out, by `co_await` or `wait()`: what `f` threw, rethrown.

## Notes

`f` runs on a thread of the pool, not a worker: it may block, and it is not a coroutine, so it does not `co_await`; it
must not `wait()` for something only the jobs behind it in the queue would give. It may `spawn_blocking` another job,
which gets a thread of its own while the pool is under its cap.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int legacy_lookup(int id) {  // a synchronous library: the call holds its thread
    this_thread::sleep_for(10ms);
    return id * 10;
}

async::task<int> lookup_all(int count) {
    vector<async::blocking_task<int>> calls;
    for (int id : range(count)) {
        calls.push_back(async::spawn_blocking([id] { return legacy_lookup(id); }));
    }
    int sum = 0;
    for (auto& call : calls) {
        sum += co_await call;  // suspended until the call returns, no thread held
    }
    co_return sum;
}

int main() {
    println("{}", async::spawn(lookup_all(20)).wait());
    println("{}", async::spawn_blocking([] { return legacy_lookup(7); }).wait());  // a thread waits
}
```

Output:

```text
1900
70
```

## See also

- [blocking_task](blocking_task/README.md): the handle
- [go_blocking](go_blocking.md): a blocking call with no handle, what it throws to on_unhandled
- [blocking_pool](blocking_pool/README.md): the threads, their cap and their idle time
- [readable](readable.md), [writable](writable.md): a wait for a descriptor that needs no thread at all, the better
  tool for a socket
- [promise](promise/README.md): what carries the result back
- [thread](../core/thread/README.md): a thread of the program's own
