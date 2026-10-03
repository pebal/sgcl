[sgcl](../../README.md) › [async](../README.md) › [wait_group](../wait_group.md)

# sgcl::async::wait_group::wait, operator co_await

```cpp
void wait() const noexcept;                 // (1)
auto operator co_await() const noexcept;    // (2)
```

Waits for the count to reach zero; at zero, returns at once. A group is waited for as a task is
([README: Waiting operations](../README.md#waiting-operations)):

1. `g.wait()` on a thread: blocks the thread until the count is zero. Not from a task on a worker, which it would
   block with every task the worker runs; debug builds assert.
2. `co_await g` in a task: the task suspends, holding no thread, and is resumed by the `done` or the `add` that
   brings the count to zero.

Either way the wait is a receive on the channel of the current round, which the count reaching zero closes; a wait
woken by the close of a round that another `add` has followed already looks at the count again and waits for the
new round.

## Parameters

None.

## Return value

1. None.
2. An [operation](../operation.md) that `co_await` carries out; the `co_await` gives nothing.

## Complexity

Constant at zero: a look at the count. Otherwise a waiter is registered on the round's channel until its close.

## Exceptions

- (1) None. A receive of a channel may wake a waiting sender, whose wake may start the scheduler's workers, but
  nobody sends on a round's channel: the count reaching zero closes it.
- (2) None from the call; the `co_await` is the same receive, and is not declared `noexcept`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> part(async::wait_group parts, int id) {
    println("part {}", id);
    parts.done();
    co_return;
}

async::task<int> assemble() {
    async::wait_group parts;
    for (int id : range(3)) {
        parts.add();
        co_await async::spawn(part(parts, id));  // one after another, for an ordered print
    }
    co_await parts;  // a task: no thread held
    co_return parts.count();
}

int main() {
    async::task<int> t = async::spawn(assemble());
    println("left: {}", t.wait());
    async::wait_group none;
    none.wait();  // a thread; at zero, at once
}
```

Output:

```text
part 0
part 1
part 2
left: 0
```

## See also

- [add](add.md), [done](done.md): what the wait waits for
- [on_done](on_done.md): the wait as a case of a select
- [sgcl::async::wait_group](../wait_group.md)
