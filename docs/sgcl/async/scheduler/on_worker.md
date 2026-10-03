[sgcl](../../README.md) › [async](../README.md) › [scheduler](../scheduler.md)

# sgcl::async::scheduler::on_worker

```cpp
static bool on_worker() noexcept;
```

Checks whether the calling thread is one of the scheduler's workers. The thread of an [executor](../executor.md),
the threads of the [blocking pool](../blocking_pool.md) and the program's own threads are not; a task on a
[strand](../strand.md) runs on a worker.

## Parameters

None.

## Return value

`true` on a worker, `false` on any other thread.

## Complexity

Constant: a read of a thread-local word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<bool> where() {
    co_return async::scheduler::on_worker();
}

int main() {
    println("main: {}", async::scheduler::on_worker());
    println("a spawned task: {}", async::spawn(where()).wait());

    async::executor ex;
    println("a task on an executor: {}", ex.run(where()));
}
```

Output:

```text
main: false
a spawned task: true
a task on an executor: false
```

## See also

- [executor](../executor.md): a task on a thread of the program's choosing
- [sgcl::async::scheduler](../scheduler.md)
