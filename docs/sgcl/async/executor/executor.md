[sgcl](../../README.md) › [async](../README.md) › [executor](../executor.md)

# sgcl::async::executor::executor

```cpp
executor() noexcept;                   // (1)
executor(const executor&) = delete;    // (2)
```

1. Constructs an executor with an empty queue, a managed object the executor holds through a root. Nothing runs
   until a thread runs the executor ([run](run.md), [poll](poll.md)).
2. An executor is not copyable, nor movable: the tasks on it hold its queue, and the executor is where a thread
   runs them.

## Parameters

None.

## Complexity

Constant: the queue, and its stub frame, made.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> hello() {
    println("hello from the executor's thread");
    co_return;
}

int main() {
    async::executor ex;
    println("{} run on an empty executor", ex.poll());
    ex.go(hello());
    println("queued, not run yet");
    println("{} run", ex.poll());
}
```

Output:

```text
0 run on an empty executor
queued, not run yet
hello from the executor's thread
1 run
```

## See also

- [run](run.md), [poll](poll.md): a thread runs the executor
- [sgcl::async::executor](../executor.md)
