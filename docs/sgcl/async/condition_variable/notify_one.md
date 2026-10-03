[sgcl](../../README.md) › [async](../README.md) › [condition_variable](../condition_variable.md)

# sgcl::async::condition_variable::notify_one

```cpp
void notify_one();
```

Wakes the first waiter, if there is one: pops it from the queue and sends it the signal on its channel. A waiter
that has not reached its receive yet keeps the signal and does not wait. With no waiter the notify is lost, so a
waiter checks its condition under the mutex before it waits.

The notify need not be made under the mutex, but a change of the condition must be, and a notify made under the
mutex after the change reaches every wait that began before it.

## Parameters

None.

## Return value

None.

## Complexity

Constant: one pop from the queue and one send.

## Exceptions

`std::system_error` when the wake of a waiting task must start the scheduler's workers and a thread cannot be
started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <mutex>

using namespace sgcl;

struct Slot {
    async::mutex lock;
    async::condition_variable filled;
    int value = 0;  // guarded by lock
};

async::task<int> take(tracked_ptr<Slot> s) {
    auto guard = co_await s->lock.scoped_lock();
    co_await s->filled.wait(guard, [&] { return s->value != 0; });
    co_return s->value;
}

int main() {
    tracked_ptr s = make_tracked<Slot>();
    async::task<int> t = async::spawn(take(s));
    {
        std::lock_guard guard(s->lock);
        s->value = 42;
        s->filled.notify_one();
    }
    println("{}", t.wait());
}
```

Output:

```text
42
```

## See also

- [notify_all](notify_all.md): wakes every waiter
- [wait](wait.md): what the notify ends
- [sgcl::async::condition_variable](../condition_variable.md)
