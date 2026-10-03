[sgcl](../../README.md) › [async](../README.md) › [blocking_task](../blocking_task.md)

# sgcl::async::blocking_task\<T\>::blocking_task

```cpp
/*(1)*/ blocking_task() noexcept = default;
/*(2)*/ blocking_task(blocking_task&& other) noexcept = default;
/*(3)*/ blocking_task(const blocking_task&) = delete;
```

1. An empty handle, holding no job: [done](done.md) says `false`, and nothing else may be called on it until a job
   is moved in.
2. Takes over the job of `other`, which is left empty.
3. A handle is not copyable: the result is moved out by one waiter.

A handle with a job is made by [spawn_blocking](../spawn_blocking.md) alone: the constructor from the job is private.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose job to take over |

## Complexity

Constant: (2) the root cell moved, not the job.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::blocking_task<int> empty;
    println("empty: done {}", empty.done());

    auto job = async::spawn_blocking([] { return 7; });
    async::blocking_task<int> moved(std::move(job));
    println("{}", moved.wait());
    println("moved from: done {}", job.done());
}
```

Output:

```text
empty: done false
7
moved from: done false
```

## See also

- [operator=](operator_assign.md): takes over another handle's job
- [spawn_blocking](../spawn_blocking.md): makes a handle with a job
- [sgcl::async::blocking_task\<T\>](../blocking_task.md)
