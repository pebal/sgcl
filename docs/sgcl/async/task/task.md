[sgcl](../../README.md) › [async](../README.md) › [task](../task.md)

# sgcl::async::task\<T\>::task

```cpp
task() noexcept = default;          // (1)
task(task&&) noexcept = default;    // (2)
```

Constructs a task object.

1. An empty task: [done](done.md) gives `true`, and [resume](resume.md), [wait](wait.md) and [result](result.md)
   may not be called. It is a place to assign a task to later.
2. Takes the coroutine of the other task over; the other task is empty after.

A task with a coroutine comes from calling a coroutine function that returns one: the call allocates the frame,
constructs the promise and the task, and suspends before the first statement of the body. The task is not copyable.
The constructors of `task<void>` are the same.

## Parameters

| Parameter | Description |
|---|---|
| `task&&` | the task whose coroutine is taken over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> answer() {
    co_return 42;
}

int main() {
    async::task<int> t;
    println("{}", t.done());

    t = answer();  // the coroutine, suspended before its first statement
    println("{}", t.done());

    async::task<int> moved = std::move(t);
    println("{}", t.done());
    println("{}", moved.wait());
}
```

Output:

```text
true
false
true
42
```

## See also

- [operator=](operator_assign.md): lets go of the task held and takes another's over
- [spawn](spawn.md): starts the task
- [sgcl::async::task\<T\>](../task.md)
