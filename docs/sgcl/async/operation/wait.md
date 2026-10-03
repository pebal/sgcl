[sgcl](../../README.md) › [async](../README.md) › [operation](../operation.md)

# sgcl::async::operation\<F\>::wait, await_ready, await_suspend, await_resume

```cpp
decltype(auto) wait() && noexcept(/* see below */);             // (1)
bool await_ready() noexcept(/* see below */);                   // (2)
template<class H>
decltype(auto) await_suspend(H h) noexcept(/* see below */);    // (3)
decltype(auto) await_resume() noexcept(/* see below */);        // (4)
```

Carry the operation out, in one of its two ways.

1. The thread's way: blocks the calling thread until the operation is done and returns its result. It is called on
   an rvalue, `ch.receive().wait()` or `std::move(op).wait()`, and goes to the operation's own blocking code, never
   through the scheduler. Not from a task on a worker, whose wait would hold the worker.
2. The coroutine's way, which `co_await op` calls: makes the awaitable of the operation in place, in the operation
   object, and asks it whether the operation is done already (a value waiting in a channel, a mutex free), in which
   case the task goes on without suspending.
3. Suspends the awaiting coroutine on the operation, with no thread held, until whatever the operation waits for
   makes it ready on the scheduler.
4. The result of the operation, given to the awaiting coroutine as the value of `co_await op`.

A program writes `co_await op` and `op.wait()`; (2)–(4) are the awaiter's members the compiler calls. The coroutine
needs a managed frame (a task, or a coroutine whose promise derives from
[managed_frame](../../core/managed_frame.md)), which holds the operation and so its awaitable for the length of the
wait. Each is noexcept when the operation's code it calls is: the blocking code for (1), the awaitable's members for
(2)–(4).

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle of the awaiting coroutine |

## Return value

- (1) What the operation gives a thread, on the page of the function that made it (an `optional<T>` for a
  channel's receive, a `bool` for its send, a guard for a mutex's scoped_lock).
- (2) `true` when the operation is done without a wait.
- (3) What the awaitable's `await_suspend` returns.
- (4) What the operation gives a task, the same as (1) gives a thread.

## Complexity

What the operation costs, on the page of the function that made it; (2) adds the construction of the awaitable in
place.

## Exceptions

What the operation's blocking code (1), or its awaitable (2–4), throws, on the page of the function that made it;
none where the signature is noexcept for that operation.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> doubler(async::channel<int> in, async::channel<int> out) {
    while (auto v = co_await in.receive()) {  // the task's way
        co_await out.send(*v * 2);
    }
    out.close();
}

int main() {
    async::channel<int> in, out;
    auto t = async::spawn(doubler(in, out));
    for (int v : {1, 2, 3}) {
        (void)in.send(v).wait();  // the thread's way
        println("{}", *out.receive().wait());
    }
    in.close();
    t.wait();
    println("{}", out.receive().wait().has_value());
}
```

Output:

```text
2
4
6
false
```

## See also

- [(constructor)](operation.md): how an operation is made and moved
- [spawn](../spawn.md): an operation run as a task of its own
- [sgcl::async::operation\<F\>](../operation.md)
