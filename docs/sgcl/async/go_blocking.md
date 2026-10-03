[sgcl](../README.md) › [async](README.md)

# sgcl::async::go_blocking

```cpp
#include "sgcl/async/blocking.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class F>
    void go_blocking(F f);
}
```

Runs a blocking call on the [blocking pool](blocking_pool.md) and lets go of it: [go](go.md)'s counterpart for a call
that is not a coroutine, for a call whose result nobody waits for. `f` runs on a thread of the pool, apart from the
workers, as under [spawn_blocking](spawn_blocking.md), and no handle is kept: what `f` returns is dropped, and what it
throws goes to [on_unhandled](on_unhandled.md)'s handler, as what a task started by `go` throws does. A
`spawn_blocking(f)` whose handle is dropped runs `f` too, but keeps what it throws in a result nobody reads, where it
is lost unseen.

`f` is moved into a job, the managed object of `spawn_blocking`, and takes the same road through the pool: the
closure may capture `tracked_ptr`s by value, traced through the job. Since nobody waits for the job, what it captures
by reference must outlive it on its own: a local of a function that returns before the job runs is gone under it.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the call: a callable with no arguments, run once on a thread of the pool |

## Return value

None.

## Complexity

Constant: the job made, a push on the pool's lock-free queue, and the pool's lock taken once to wake an idle thread
or start one, as for [spawn_blocking](spawn_blocking.md).

## Exceptions

What the move constructor of `F` throws, and `std::system_error` when the pool has no thread and cannot start one.
Then `f` never runs, and nothing of the job is left behind. A pool that has a thread but cannot start another throws
nothing: the job waits in the queue for a thread to free up, as at the cap.

What `f` throws, nobody reads: it goes to [on_unhandled](on_unhandled.md)'s handler, once, on the thread of the pool
that ran `f`. The default handler prints it, naming the blocking pool as the place, and ends the program, as for a
task started by [go](go.md).

## Notes

`f` runs on a thread of the pool, not a worker: it may block, and it is not a coroutine, so it does not `co_await`;
it must not `wait()` for something only the jobs behind it in the queue would give. A job that is to stop early is
given a [stop_token](stop_token.md) and looks at it.

[blocking_pool::wait_idle](blocking_pool/wait_idle.md) waits for every job queued so far, these included;
[blocking_pool::stop](blocking_pool/stop.md), [scheduler::stop](scheduler/stop.md) and the end of the program run
the jobs queued to their end, as they do those of `spawn_blocking`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <atomic>
#include <stdexcept>

using namespace sgcl;
using namespace std::chrono_literals;

std::atomic<int> saved = 0;

void save_report(int id) {  // a synchronous library: the call holds its thread
    this_thread::sleep_for(10ms);
    if (id == 3) {
        throw std::runtime_error("disk full");
    }
    ++saved;
}

void log_it(std::exception_ptr e) {
    try {
        std::rethrow_exception(e);
    } catch (const std::exception& x) {
        println("a job failed: {}", x.what());
    }
}

int main() {
    async::on_unhandled(log_it);
    for (int id : range(5)) {
        async::go_blocking([id] { save_report(id); });  // nobody waits for it
    }
    async::blocking_pool::wait_idle();
    println("saved {}", saved.load());
}
```

Output:

```text
a job failed: disk full
saved 4
```

## See also

- [go](go.md): a task started and let go of
- [spawn_blocking](spawn_blocking.md): a blocking call whose handle is kept, for the result
- [on_unhandled](on_unhandled.md): what becomes of what `f` throws
- [blocking_pool](blocking_pool.md): the threads, their cap and their idle time
