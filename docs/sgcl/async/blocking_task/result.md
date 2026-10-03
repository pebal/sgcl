[sgcl](../../README.md) › [async](../README.md) › [blocking_task](README.md)

# sgcl::async::blocking_task\<T\>::result

```cpp
decltype(auto) result();
```

Gives the result of the job in place, as a task's `result()` does: a reference to what the job's function returned,
or what it threw, rethrown. When the job has not run yet, the calling thread waits for it first, as
[wait](wait.md) does: not from a task on a worker.

## Parameters

None.

## Return value

`T&`, a reference into the job, which stays valid while the job is held; nothing for a `blocking_task<void>`.

## Complexity

Constant, besides the wait for a job that has not run.

## Exceptions

What the job's function threw, rethrown, at every call.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto job = async::spawn_blocking([] { return string("a long report"); });
    println("{} characters", job.result().size());
    println("{}", job.result());  // read again: the result stays in the job
}
```

Output:

```text
13 characters
a long report
```

## See also

- [wait, operator co_await](wait.md): the result moved out
- [sgcl::async::blocking_task\<T\>](README.md)
