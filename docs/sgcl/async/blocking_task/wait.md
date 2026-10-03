[sgcl](../../README.md) › [async](../README.md) › [blocking_task](../blocking_task.md)

# sgcl::async::blocking_task\<T\>::wait, operator co_await

```cpp
/*(1)*/ T wait();
/*(2)*/ awaiter operator co_await() noexcept;
```

Waits for the job and gives its result: what the job's function returned, moved out of the job, or what it threw,
rethrown.

1. On a thread: blocks the calling thread until the job ran. Not from a task on a worker, which it would take from
   every other task: debug builds assert.
2. In a task: `co_await job` suspends the task until the job ran, with no thread held, and resumes it on its worker,
   or on its [executor](../executor.md).

## Parameters

None.

## Return value

- (1) What the function returned (nothing for a `blocking_task<void>`).
- (2) The awaitable; `co_await` gives the same.

## Complexity

Constant, besides the wait for the job.

## Exceptions

- (1) What the job's function threw, rethrown.
- (2) The call: none. Carried out, by `co_await`: what the job's function threw, rethrown.

## Notes

The result is moved out of the job: the handle is waited for once. [result](result.md) reads it in place.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> in_a_task() {
    co_return co_await async::spawn_blocking([] { return 2; });
}

int main() {
    println("{}", async::spawn_blocking([] { return 1; }).wait());
    println("{}", async::spawn(in_a_task()).wait());
    async::spawn_blocking([] { println("a job of nothing"); }).wait();
}
```

Output:

```text
1
2
a job of nothing
```

## See also

- [result](result.md): the result read in place
- [on_done](on_done.md): the wait as a case of a select
- [sgcl::async::blocking_task\<T\>](../blocking_task.md)
