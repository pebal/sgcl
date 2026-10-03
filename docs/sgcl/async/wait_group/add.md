[sgcl](../../README.md) › [async](../README.md) › [wait_group](README.md)

# sgcl::async::wait_group::add

```cpp
void add(long n = 1) const;
```

Adds `n` to the count, as Go's `WaitGroup.Add` does.

- A positive `n` counts work in. An `add` that takes the count up from zero starts a new round: the waiters of the
  last one have been released and keep its closed channel, and the new round's channel is made for the waits to
  come.
- A negative `n` takes work off, as [done](done.md) takes one: the `add` that brings the count to zero closes the
  round's channel, releasing every waiter.
- Zero does nothing.

The `add` of the work comes before the work starts and before the wait for it, in the program's order: an `add`
from zero racing with a wait is a misuse, as it is in Go.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the work counted in, or taken off when negative; one by default |

## Return value

None.

## Complexity

Constant: an atomic add, plus the new round's channel for an `add` from zero, or the wake of every waiter for one
that brings the count to zero.

## Exceptions

`std::system_error` when a negative `n` brings the count to zero, the close wakes a waiting task, the wake must
start the scheduler's workers and a thread cannot be started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::wait_group jobs;
    jobs.add(5);
    jobs.add();
    println("{}", jobs.count());
    jobs.add(-6);  // all taken off at once: the waiters released
    jobs.wait();
    println("{}", jobs.count());
}
```

Output:

```text
6
0
```

## See also

- [done](done.md): counts one off
- [count](count.md): the work not yet counted off
- [wait, operator co_await](wait.md): waits for zero
- [sgcl::async::wait_group](README.md)
