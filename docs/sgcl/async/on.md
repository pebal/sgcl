[sgcl](../README.md) › [async](README.md)

# sgcl::async::on

```cpp
#include "sgcl/async/executor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class [[nodiscard]] on {
    public:
        explicit on(executor& ex) noexcept;
        explicit on(strand& s) noexcept;

        bool await_ready() const noexcept;

        template<class P>
        bool await_suspend(std::coroutine_handle<P> h);

        void await_resume() const noexcept;
    };
}
```

An awaitable that moves the task to an [executor](executor.md) or a [strand](strand.md): after
`co_await async::on(ex)` the task goes on on `ex` from the next line, and whatever it awaits from then on wakes it
there. A task already there goes on at once, without a hop. It works from anywhere: from the pool, from another
executor or strand, from a thread that is no worker and no executor (a task resumed by hand). Kotlin's
`withContext(Dispatchers.Main)` with the rest of the block after it; [on_workers](on_workers.md) is the way back
to the pool.

The frame's header is set to the executor and the frame is pushed on its queue; from the push on, the executor's
thread may run it, so the task is never on two threads at once.

## Parameters

| Parameter | Description |
|---|---|
| `ex` | the executor the task goes on on |
| `s` | the strand the task goes on on |

## Return value

An awaitable. `co_await async::on(ex)` gives nothing; the task is on `ex` when it returns. There is no form for a
thread.

## Complexity

Constant: a push on the executor's queue (an exchange of the tail and a store of a link), with no allocation, and a
wake of its thread or, for a strand, the hand-off to the workers when the strand was idle.

## Exceptions

- The construction: none.
- The `co_await` to an executor: none. To a strand: `std::system_error` when the hand-off to the workers has to
  start the scheduler and a worker thread cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

thread::id main_thread;

async::task<int> handler(async::executor& ui) {
    co_await async::on_workers();
    int pixels = 640 * 480;  // the heavy part on the pool
    println("decoded on a worker: {}", async::scheduler::on_worker());
    co_await async::on(ui);
    println("shown on the main thread: {}", this_thread::get_id() == main_thread);
    co_return pixels;
}

int main() {
    main_thread = this_thread::get_id();
    async::executor ui;
    println("{} pixels", ui.run(handler(ui)));
}
```

Output:

```text
decoded on a worker: true
shown on the main thread: true
307200 pixels
```

## See also

- [on_workers](on_workers.md): back to the pool
- [executor](executor.md), [strand](strand.md): where a task goes
- [yield](yield.md): to the back of the queue it is on
