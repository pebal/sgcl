[sgcl](../../README.md) › [async](../README.md) › [task](../task.md)

# sgcl::async::task\<T\>::spawn

```cpp
[[nodiscard]] task& spawn();
```

Puts the task on the scheduler's queue of the ready and returns at once; a worker of the pool runs the coroutine to
its next suspension. The task takes the task-locals of the task the calling thread runs, if any. The free function
[spawn](../spawn.md), `auto t = async::spawn(f());`, is this call with the task given back; it is the usual
spelling.

A task is spawned once, before it runs: not an empty one, not one that is done, not one already started by a
`spawn`, a wait or a `resume` (debug builds assert it). The result is `[[nodiscard]]`: a task whose handle nobody
keeps is started with [go](../go.md), which says so. A spawned task whose object is dropped all the same runs on to
its end ([detach](detach.md)).

## Parameters

None.

## Return value

`*this`.

## Complexity

Constant: a push on a queue of the scheduler, and the wake of a sleeping worker when none is looking for work.

## Exceptions

`std::system_error` when the push starts the scheduler (the first start, or the first after a stop) and a worker's
thread cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> square(int x) {
    co_return x * x;
}

int main() {
    async::task<int> t = square(7);
    println("{}", t.done());
    println("{}", t.spawn().wait());  // spawned, then waited for
}
```

Output:

```text
false
49
```

## See also

- [spawn](../spawn.md): the free function, which returns the task
- [go](../go.md): a spawn whose handle nobody keeps
- [resume](resume.md): runs the coroutine by hand instead
- [sgcl::async::task\<T\>](../task.md)
