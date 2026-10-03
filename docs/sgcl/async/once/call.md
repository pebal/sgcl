[sgcl](../../README.md) › [async](../README.md) › [once](../once.md)

# sgcl::async::once::call

```cpp
/*(1)*/ template<class F>
        auto call(F f) noexcept(std::is_nothrow_move_constructible_v<F>);
/*(2)*/ template<class T>
        task<> call(task<T> t);
/*(3)*/ template<class F>
        task<> call(F f);
```

Runs the function by the first caller; every other caller waits for it to finish. The first is the caller whose
call, carried out, claims the once; the others wait on the channel its end closes, and none of them runs anything.

1. `f()`, a function that is not a coroutine. The call does nothing yet; it returns an
   [operation](../operation.md), carried out in one of two ways
   ([README: Waiting operations](../README.md#waiting-operations)): `co_await o.call(f)` in a task, where `f` runs
   in the first caller's task and the others wait holding no thread, and `o.call(f).wait()` on a thread, not from
   a task on a worker (debug builds assert). Takes part only when `F` is not a coroutine function as (3) takes.
2. The task `t`: the [task](../task.md) returned awaits `t` when it claims the once, and waits for the first
   caller's otherwise; the `t` of a caller that lost is never started. `co_await o.call(t)` in a task,
   `o.call(t).wait()` on a thread.
3. A coroutine function given uncalled, a lambda that returns a task, captures and all: the closure is copied into a
   frame that lives as long as its task, and that task is run as (2) runs `t`. A function's form would call the
   lambda and drop the task it made, never started. Takes part only when `f()` gives a coroutine type, a task.

- (1–3) A function or a task that throws ends the once all the same: the exception is kept, the once is done, and
  the first caller gets it thrown, as does every caller then waiting or coming later. Nothing is run again.

## Parameters

| Parameter | Description |
|---|---|
| `f` | (1) the function run by the first caller; (3) the coroutine function whose task is run by the first caller |
| `t` | the task run by the first caller |

## Return value

- (1) An [operation](../operation.md). Carried out, by `co_await` or by `.wait()`, it gives nothing, once the
  function has run, here or by the first caller.
- (2–3) A [task](../task.md), awaited with `co_await` or `.wait()`; it gives nothing, once the task has run, here or
  by the first caller.

## Complexity

The first caller: the function, or the task. Every other: a wait on the once's channel, constant once the call is
done. In a task, (1) carried out makes a task of its own, one frame on the managed heap.

## Exceptions

- (1) The call: what the move constructor of `F` throws; none when it is noexcept. (3) The call: what the move
  constructor of `F` throws.
- Carried out: what `f` or `t` throws, at the first caller and again at every other; `std::system_error` when the
  end of the call wakes a waiting task, the wake must start the scheduler's workers and a thread cannot be started
  ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> open_pool() {
    println("connecting");
    co_return 3;
}

async::task<> user(async::once& init, int id) {
    co_await init.call(open_pool());  // the task of the first caller alone runs
    println("user {} connected", id);
}

int main() {
    async::once init;
    for (int id : range(3)) {
        async::spawn(user(init, id)).wait();
    }

    async::once greeting;
    for (int i : range(2)) {
        greeting.call([] { println("hello"); }).wait();  // a thread
    }

    async::once lazy;
    string name = "cache";
    lazy.call([name]() -> async::task<> {  // uncalled: the captures live with the task
        println("{} warmed", name);
        co_return;
    }).wait();
}
```

Output:

```text
connecting
user 0 connected
user 1 connected
user 2 connected
hello
cache warmed
```

## See also

- [called](called.md): whether the call is done
- [promise](../promise.md): a value set once
- [sgcl::async::once](../once.md)
