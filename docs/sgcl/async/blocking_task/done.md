[sgcl](../../README.md) › [async](../README.md) › [blocking_task](../blocking_task.md)

# sgcl::async::blocking_task\<T\>::done

```cpp
bool done() const noexcept;
```

Checks whether the job has run: its value, or the exception its function threw, is in. It does not wait.

## Parameters

None.

## Return value

`true` once the job ran; `false` while it is queued or running, and for a handle that holds no job.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::event go;
    auto job = async::spawn_blocking([go] {
        go.wait();  // the job holds its thread until the event is set
        return 1;
    });
    println("before: {}", job.done());
    go.set();
    job.wait();
    println("after: {}", job.done());
}
```

Output:

```text
before: false
after: true
```

## See also

- [wait, operator co_await](wait.md): waits for the job
- [sgcl::async::blocking_task\<T\>](../blocking_task.md)
