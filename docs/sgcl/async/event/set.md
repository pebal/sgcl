[sgcl](../../README.md) › [async](../README.md) › [event](README.md)

# sgcl::async::event::set

```cpp
void set() const;
```

Sets the event: the close of its channel, which wakes every thread and task waiting for it and serves every select
with an [on_set](on_set.md) case on it. Every wait after the set returns at once. A second set does nothing: an
event is set once, and there is no reset.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of waiters, each woken once.

## Exceptions

`std::system_error` when the wake of a waiting task must start the scheduler's workers and a thread cannot be
started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> waiter(async::event ready, atomic<int>& woken) {
    co_await ready;
    ++woken;
}

int main() {
    async::event ready;
    atomic<int> woken = 0;
    vector<async::task<>> waiters;
    for (int i : range(5)) {
        waiters.push_back(async::spawn(waiter(ready, woken)));
    }
    ready.set();
    ready.set();  // set already: nothing
    for (auto& w : waiters) {
        w.wait();
    }
    println("{} woken", woken.load());
}
```

Output:

```text
5 woken
```

## See also

- [is_set](is_set.md): whether the set has happened
- [wait, operator co_await](wait.md), [on_set](on_set.md): wait for the set
- [sgcl::async::event](README.md)
