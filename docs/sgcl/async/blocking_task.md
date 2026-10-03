[sgcl](../README.md) › [async](README.md)

# sgcl::async::blocking_task\<T\>

```cpp
#include "sgcl/async/blocking.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    class blocking_task {
    public:
        class awaiter;
    };
}
```

`sgcl::async::blocking_task<T>` is the handle of a job on the [blocking pool](blocking_pool.md), made by
[spawn_blocking](spawn_blocking.md): `co_await` gives what the job's function returned, or rethrows what it threw,
and so does [wait](blocking_task/wait.md) on a thread; [on_done](blocking_task/on_done.md) is a case of a
[select](select.md). It is a [task](task.md) for a call that is not a coroutine: the job runs on a thread of the pool
whether or not the handle is kept, and a handle dropped is a job whose result nobody reads.

The handle holds its job through a `root_ptr`, as a task holds its frame, so it lives anywhere, a `std::vector` of
handles included, at the cost of a root cell per handle; it is move-only.

## Rules

- The result is in the job: [wait](blocking_task/wait.md) and `co_await` move it out, once;
  [result](blocking_task/result.md) gives a reference to it.
- [wait](blocking_task/wait.md) blocks the calling thread: not from a task on a worker (debug builds assert), where
  the handle is `co_await`ed.
- A default-constructed handle, or one moved from, holds no job: only [done](blocking_task/done.md) may be called
  on it, and it says `false`.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | What the job's function returns, decayed; `void` for a function that returns nothing. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `awaiter` | the awaitable of `co_await` on the handle ([wait, operator co_await](blocking_task/wait.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](blocking_task/blocking_task.md) | constructs an empty handle, or takes over another's job |
| `(destructor)` | lets go of the job, which runs on all the same |
| [operator=](blocking_task/operator_assign.md) | takes over another handle's job |

#### Observers

| Function | Description |
|---|---|
| [done](blocking_task/done.md) | checks whether the job ran |

#### Waiting

| Function | Description |
|---|---|
| [wait, operator co_await](blocking_task/wait.md) | waits for the job: its result, or what it threw |
| [on_done](blocking_task/on_done.md) | a case of a select, served once the job ran |

#### Getting the result

| Function | Description |
|---|---|
| [result](blocking_task/result.md) | a reference to the result, waited for first |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> report(async::blocking_task<int> job) {
    try {
        println("got {}", co_await job);
    } catch (const std::exception& e) {
        println("failed: {}", e.what());
    }
}

int main() {
    auto good = async::spawn_blocking([] {
        this_thread::sleep_for(5ms);
        return 42;
    });
    auto bad = async::spawn_blocking([]() -> int {
        throw std::runtime_error("no such host");
    });
    async::spawn(report(std::move(good))).wait();
    async::spawn(report(std::move(bad))).wait();
}
```

Output:

```text
got 42
failed: no such host
```

## See also

- [spawn_blocking](spawn_blocking.md): what makes the handle
- [blocking_pool](blocking_pool.md): where the job runs
- [task](task.md): the handle of a coroutine
- [promise](promise.md): what carries the result back
