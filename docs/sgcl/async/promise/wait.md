[sgcl](../../README.md) › [async](../README.md) › [promise](../promise.md)

# sgcl::async::promise\<T\>::wait, operator co_await

```cpp
/*(1)*/ T& wait() const;
/*(2)*/ awaiter operator co_await() const noexcept;
/*(3)*/ void wait() const;  // promise<void>
```

Waits for the promise to be set and gives its result: the value, or the exception [set_exception](set_exception.md)
set, rethrown. A promise is waited for as a task is, on itself: `p.wait()` on a thread, `co_await p` in a task
([README: Waiting operations](../README.md#waiting-operations)).

1. Blocks the calling thread until the promise is set, then gives the value.
2. `co_await p`: the task is suspended until the promise is set, with no thread held, then made ready on the scheduler
   by the side that sets it; the `co_await` gives the value, a `T&`, or for `promise<void>` nothing.
3. `promise<void>`: blocks the calling thread until the promise is set, and gives nothing.

Any number of threads and tasks may wait for one promise; a wait on a set promise does not wait.

## Parameters

None.

## Return value

- (1) A reference to the value in the promise's state: every waiter reads the one value, and a lone reader may move
  it out.
- (2) The [awaiter](../promise.md#member-types) of `co_await p`, which gives what (1) or (3) gives.
- (3) None.

## Complexity

Constant on a set promise. A wait that waits allocates its waiter, one managed object, on the promise's channel.

## Exceptions

- (1), (3) What `set_exception` set, rethrown.
- (2) The call: none. The `co_await`: what `set_exception` set, rethrown.

## Notes

`wait()` is for a thread, never for a task on a worker, where it would hold the worker from every other task: a debug
build asserts on a worker even when the promise is set already. A task writes `co_await p`, and a coroutine that
does needs a managed frame ([README: Coroutines](../README.md#coroutines)).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> twice(async::promise<int> p) {
    int v = co_await p;  // no thread held until the set
    co_return v * 2;
}

int main() {
    async::promise<int> p;
    async::task<int> doubling = async::spawn(twice(p));
    thread setter([p] { p.set_value(21); });

    println("{}", p.wait());  // this thread blocks
    println("{}", doubling.wait());
    setter.join();
}
```

Output:

```text
21
42
```

## See also

- [result](result.md): the value without a wait when the promise is set
- [on_done](on_done.md): the wait as a case of a select
- [set_value](set_value.md), [set_exception](set_exception.md): what ends the wait
- [sgcl::async::promise\<T\>](../promise.md)
