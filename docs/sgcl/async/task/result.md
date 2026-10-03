[sgcl](../../README.md) › [async](../README.md) › [task](../task.md)

# sgcl::async::task\<T\>::result

```cpp
/*(1)*/ T& result();
/*(2)*/ void result();
```

The value the coroutine `co_return`ed, or what it threw, rethrown, every time `result()` is called.

1. A reference to the value, which stays in the frame: it may be read again.
2. `task<void>`: nothing, or the exception.

A task that is not done yet is waited for first, on the calling thread, as [wait](wait.md) waits (a task nobody
started is put on the scheduler), so a task on a worker calls it only on a task that is done, and `co_await`s one
that is not. A task driven by hand with [resume](resume.md) is read once it is [done](done.md): nobody else resumes
it. The task may not be empty.

## Parameters

None.

## Return value

- (1) A reference to the value in the frame, valid while the task object holds the frame.
- (2) None.

## Complexity

Constant for a task that is done; otherwise the time it takes to end.

## Exceptions

What the coroutine threw, rethrown; `std::system_error` when the wait for a task not done starts the scheduler and a
worker's thread cannot be started.

## Notes

An exception that `result()`, `wait()` or `co_await` gave to someone is theirs: it never goes to
[on_unhandled](../on_unhandled.md)'s handler, even when the task is let go of after.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

async::task<string> greeting(string name) {
    if (name.empty()) {
        throw std::invalid_argument("no name");
    }
    co_return "hello, " + name;
}

int main() {
    auto t = async::spawn(greeting("Ada"));
    t.wait();
    println("{}", t.result());
    println("{}", t.result());  // the value stays in the frame

    auto u = async::spawn(greeting(""));
    for (int i : range(2)) {
        try {
            u.result();  // waits for the task the first time
        } catch (const std::exception& e) {
            println("{}", e.what());
        }
    }
}
```

Output:

```text
hello, Ada
hello, Ada
no name
no name
```

## See also

- [wait, operator co_await](wait.md): waits for the task and gives the value
- [done](done.md): checks whether the task has ended
- [sgcl::async::task\<T\>](../task.md)
