[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md)

# sgcl::async::mutex::lock

```cpp
void lock() const noexcept;
```

Locks the mutex, blocking the calling thread until the mutex is free: a receive of the channel's one signal. The
standard's Lockable member, for `std::lock_guard<sgcl::async::mutex>` and `std::unique_lock` on a thread.

A task does not call it: a blocking call on a worker holds the worker and every task it would run
([README: The rules](../README.md#the-rules), 1). A task writes `co_await m.scoped_lock()`, which holds no thread
while it waits ([scoped_lock](scoped_lock.md)).

## Parameters

None.

## Return value

None.

## Complexity

Constant when the mutex is free. Otherwise the thread registers a waiter on the channel's list and parks until an
[unlock](unlock.md) hands it the signal.

## Exceptions

None. A receive of a channel may wake a waiting sender, whose wake may start the scheduler's workers, but the
mutex's channel never has one: [unlock](unlock.md) never waits.

## Notes

The lock has no owner: the thread that locks need not be the one that unlocks, and a second `lock` by the holder
waits for ever.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <mutex>

using namespace sgcl;

int main() {
    async::mutex m;
    int total = 0;  // guarded by m
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&] {
            for (int i : range(1000)) {
                std::lock_guard guard(m);  // lock() here, unlock() at the end of the scope
                ++total;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{}", total);
}
```

Output:

```text
4000
```

## See also

- [try_lock](try_lock.md): the lock without the wait
- [unlock](unlock.md): gives the mutex back
- [scoped_lock](scoped_lock.md): the lock of a task
- [sgcl::async::mutex](../mutex.md)
