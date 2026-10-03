[sgcl](../README.md) › [async](README.md)

# sgcl::async::operation\<F\>

```cpp
#include "sgcl/async/operation.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class F>
    class [[nodiscard]] operation;
}
```

`sgcl::async::operation<F>` is an operation that may wait, under one name for both ways of waiting: `co_await op` in
a task suspends the task and gives its worker back, `op.wait()` on a thread blocks the thread. It is what the
waiting members of the module return: a channel's [send](channel/send.md) and [receive](channel/receive.md), a
mutex's [scoped_lock](mutex/scoped_lock.md), a condition variable's [wait](condition_variable/wait.md), a once's
[call](once/call.md), a stop token's [stopped](stop_token/stopped.md), and the others of their kind; so a program
writes `co_await ch.receive()` in a task and `ch.receive().wait()` on a thread, and the page of each function says
what both give ([README: Waiting operations](README.md#waiting-operations)).

An operation is a description until it is carried out: making one does nothing, hence `[[nodiscard]]`. Carried out
by a thread, it goes to the operation's own blocking code, never through the scheduler, so a thread that waits pays
what the call alone costs; carried out by a task, it makes the awaitable of the operation in place, in the operation
object, which lives in the task's frame for the length of the wait.

## Rules

- An operation is carried out once, one way or the other: `co_await` in a coroutine with a managed frame, or
  `wait()` on an rvalue, `x.f().wait()` or `std::move(op).wait()`, on a thread. `wait()` inside a task would hold
  the worker and every task it would run.
- An operation made and never carried out did nothing, and a debug build asserts where it is dropped. A `(void)`
  cast silences `[[nodiscard]]` and leaves the operation undone.
- Move-constructible, not copyable, not assignable. An operation may be kept, passed to a function, or given to
  [spawn](spawn.md), which runs it as a task of its own.

## Template parameters

| Parameter | Description |
|---|---|
| `F` | The callable the function that made the operation gives it: called with one tag of the library for the awaitable and with another for the blocking wait. The constructor from it is private: a program does not make an operation itself, it gets one from a function of the module. |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](operation/operation.md) | takes another operation over |
| `(destructor)` | ends the operation; debug builds assert on one that was never carried out |

#### Waiting

| Function | Description |
|---|---|
| [wait, await_ready, await_suspend, await_resume](operation/wait.md) | carry the operation out: blocking a thread, or suspending a task with `co_await` |

## Complexity

Making an operation is the move of its callable. Carrying it out costs what the operation costs, on the page of the
function that made it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> first(async::channel<int> ch) {
    auto v = co_await ch.receive();  // the task suspends, the worker runs others
    co_return *v;
}

int main() {
    async::channel<int> ch(2);
    auto send = ch.send(1);  // a description: nothing is sent yet
    println("{}", ch.size());
    println("{}", std::move(send).wait());  // carried out on this thread
    println("{}", ch.size());

    println("{}", first(ch).wait());
    (void)ch.send(2).wait();
    println("{}", *ch.receive().wait());
}
```

Output:

```text
0
true
1
1
2
```

## See also

- [spawn](spawn.md): an operation run as a task of its own
- [select](select.md): a wait for the first of several cases, carried out the same two ways
- [task](task.md): a coroutine that awaits operations
- [README: Waiting operations](README.md#waiting-operations)
