[sgcl](../../README.md) › [async](../README.md) › [task_group](../task_group.md)

# sgcl::async::task_group::go

```cpp
template<class T>
void go(task<T> t);    // (1)
template<class F>
void go(F f);          // (2)
```

Starts a child in the group: on the scheduler, counted, with no handle given back, as [go](../go.md) starts a task
([spawn](../spawn.md) is the name of the forms that hand one back). From then on the group owns the child: its
result, if it has one, is dropped, and its exception is the group's, the first one requesting the stop of the
scope and rethrown by the wait.

1. The task `t`, one nobody spawned or one spawned already.
2. A coroutine function given uncalled, a lambda that returns a task, captures and all: the closure is copied into a
   frame that lives as long as its task, which is started as (1) starts `t`. Takes part only when `f()` gives a
   coroutine type, a task.

- (1–2) The child is awaited by a runner, a small task run on the calling thread to its first suspension, which
  puts the child on the scheduler; the runner takes the calling task's executor and task-locals first, so the
  child inherits them, as a task started by a task does. The child's end resumes the runner, which records the
  exception, if any, and counts the child off.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the child |
| `f` | the coroutine function whose task is the child |

## Return value

None.

## Complexity

Constant: the runner's frame, and the child put on the scheduler. A child started from a task goes on its worker's
own ring; one started from a thread, on the global queue.

## Exceptions

`std::system_error` when the start must start the scheduler's workers and a thread cannot be started
([README: The rules](../README.md#the-rules), 5). (2) Also what the move constructor of `F` throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> square(async::channel<int> out, int n) {
    co_await out.send(n * n);
    co_return n * n;  // dropped: the group keeps no result
}

int main() {
    async::channel<int> out(10);
    async::task_group g;
    for (int n : range(1, 5)) {
        g.go(square(out, n));
    }
    int offset = 100;
    g.go([out, offset]() -> async::task<> {  // uncalled: the captures live with the task
        co_await out.send(offset);
    });
    g.wait();
    out.close();
    int sum = 0;
    while (auto v = out.try_receive()) {
        sum += *v;
    }
    println("{}", sum);
}
```

Output:

```text
130
```

## See also

- [token](token.md): the token to give the children
- [wait, operator co_await](wait.md): waits for every child
- [go](../go.md): a task started outside a group
- [sgcl::async::task_group](../task_group.md)
