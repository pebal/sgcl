[sgcl](../../README.md) › [async](../README.md) › [task](../task.md)

# sgcl::async::task\<T\>::resume

```cpp
void resume() noexcept;
```

Runs the coroutine on the calling thread to its next suspension or to its end, as a `std::coroutine_handle` would:
a task driven by hand, without the scheduler. The coroutine gives control back to the resumer with
`co_await std::suspend_always{}`. On its first run the frame takes the executor and the task-locals of the task the
calling thread runs, if any, as a task started on the scheduler takes its starter's: what it starts inherits them,
and a wait of its own brings it back where the resumer runs.

The task may not be empty, done or spawned. An exception the coroutine throws does not leave `resume()`: the promise
keeps it, the task is [done](done.md), and [result](result.md) rethrows it.

## Parameters

None.

## Return value

None.

## Complexity

What the coroutine runs to its next suspension.

## Exceptions

None.

## Notes

A task driven by hand that gives control back with `std::suspend_always` is resumed by nobody else: its
[result](result.md) is read once it is `done()`, and a `wait()` or a `result()` before that would block for good. A
wait of its own (a channel, a timer) is another matter: what it waits for wakes it on the scheduler, a worker runs it
on from there, and a `wait()` sees its end.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <coroutine>

using namespace sgcl;

async::task<int> count_to(int n) {
    int i = 0;
    while (i < n) {
        ++i;
        println("step {}", i);
        co_await std::suspend_always{};  // back to the resumer; the frame keeps i
    }
    co_return i;
}

int main() {
    async::task<int> t = count_to(3);
    while (!t.done()) {
        t.resume();
    }
    println("{}", t.result());
}
```

Output:

```text
step 1
step 2
step 3
3
```

## See also

- [spawn](spawn.md): runs the task on the scheduler instead
- [done](done.md), [result](result.md): whether it ended, and its value
- [frame_ptr::resume](../../core/frame_ptr/resume.md): the managed frame's resume
- [sgcl::async::task\<T\>](../task.md)
