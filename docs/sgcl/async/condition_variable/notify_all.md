[sgcl](../../README.md) › [async](../README.md) › [condition_variable](../condition_variable.md)

# sgcl::async::condition_variable::notify_all

```cpp
void notify_all();
```

Wakes every waiter: pops them from the queue one by one and sends each the signal on its channel. A wait that
begins after the notify is not woken by it. Each waiter takes the mutex back in its turn, so they go on one at a
time.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of waiters: a pop and a send each.

## Exceptions

`std::system_error` when the wake of a waiting task must start the scheduler's workers and a thread cannot be
started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <mutex>

using namespace sgcl;

struct Gate {
    async::mutex lock;
    async::condition_variable opened;
    bool open = false;  // guarded by lock
};

async::task<> visitor(tracked_ptr<Gate> g) {
    auto guard = co_await g->lock.scoped_lock();
    co_await g->opened.wait(guard, [&] { return g->open; });
}

int main() {
    tracked_ptr g = make_tracked<Gate>();
    vector<async::task<>> visitors;
    for (int i : range(4)) {
        visitors.push_back(async::spawn(visitor(g)));
    }
    {
        std::lock_guard guard(g->lock);
        g->open = true;
        g->opened.notify_all();
    }
    for (auto& v : visitors) {
        v.wait();
    }
    println("{} through", visitors.size());
}
```

Output:

```text
4 through
```

## See also

- [notify_one](notify_one.md): wakes the first waiter
- [wait](wait.md): what the notify ends
- [sgcl::async::condition_variable](../condition_variable.md)
