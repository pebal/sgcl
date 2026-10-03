[sgcl](../README.md) › [async](README.md)

# sgcl::async::on_workers

```cpp
#include "sgcl/async/executor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    // `co_await sgcl::async::on_workers()`: the task goes on on the pool of
    // workers, from the next line; at once when it is on a worker with no
    // executor. A task that left the main thread for a computation
    struct [[nodiscard]] on_workers {
        bool await_ready() const noexcept;

        template<class P>
        bool await_suspend(std::coroutine_handle<P> h);

        void await_resume() const noexcept;
    };
}
```

An awaitable that moves the task to the pool of workers of the [scheduler](scheduler/README.md): after
`co_await async::on_workers()` the task goes on on a worker from the next line, and whatever it awaits from then on
wakes it on the pool. A task on a worker with no executor goes on at once, without a hop. It is the way back from an
[executor](executor/README.md) or a [strand](strand/README.md): a task that left the main thread for a computation. Kotlin's
`withContext(Dispatchers.Default)`; [on](on.md) is the way to an executor.

## Parameters

None.

## Return value

An awaitable. `co_await async::on_workers()` gives nothing; the task is on a worker when it returns. There is no
form for a thread.

## Complexity

Constant: an enqueue on the pool's queues, with no allocation.

## Exceptions

- The construction: none.
- The `co_await`: `std::system_error` when the enqueue has to start the scheduler and a worker thread cannot be
  started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<long> sum_on_the_pool(int n) {
    println("on a worker before: {}", async::scheduler::on_worker());
    co_await async::on_workers();
    println("on a worker after: {}", async::scheduler::on_worker());
    long sum = 0;
    for (int i : range(n)) {
        sum += i;
    }
    co_return sum;
}

int main() {
    async::executor main;
    println("{}", main.run(sum_on_the_pool(100)));
}
```

Output:

```text
on a worker before: false
on a worker after: true
4950
```

## See also

- [on](on.md): to an executor or a strand
- [scheduler](scheduler/README.md): the pool of workers
- [executor](executor/README.md): a task on a thread of the program's choosing
