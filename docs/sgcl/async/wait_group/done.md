[sgcl](../../README.md) › [async](../README.md) › [wait_group](../wait_group.md)

# sgcl::async::wait_group::done

```cpp
void done() const;
```

Counts one piece of work off, Go's `WaitGroup.Done`: the `done` that brings the count to zero closes the round's
channel, releasing every waiter. The round is read before the count is taken off: once the count is zero a wait may
have returned and the group may be gone, so the last `done` touches nothing of the group after its decrement. A
`done` without its `add` is a misuse, which nothing detects.

## Parameters

None.

## Return value

None.

## Complexity

Constant: an atomic subtract, and the wake of every waiter for the `done` that brings the count to zero.

## Exceptions

`std::system_error` when the close wakes a waiting task, the wake must start the scheduler's workers and a thread
cannot be started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> worker(async::wait_group all, atomic<int>& sum, int n) {  // the group by value
    sum += n;
    all.done();
    co_return;
}

int main() {
    async::wait_group all;
    atomic<int> sum = 0;
    for (int n : range(1, 11)) {
        all.add();
        async::go(worker(all, sum, n));
    }
    all.wait();
    println("{}", sum.load());
}
```

Output:

```text
55
```

## See also

- [add](add.md): counts work in, or off
- [wait, operator co_await](wait.md): waits for zero
- [sgcl::async::wait_group](../wait_group.md)
