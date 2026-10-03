[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](README.md)

# sgcl::async::shared_mutex::lock_shared

```cpp
void lock_shared() noexcept;
```

Locks the mutex for a reader, blocking the calling thread while a writer holds it or waits for it. The reader is
counted in by an atomic add on the word; when the writer's bit is set, the thread waits on the channel of that
writer's round, which the writer's [unlock](unlock.md) closes. The standard's SharedLockable member, for
`std::shared_lock<sgcl::async::shared_mutex>` on a thread.

A task does not call it: a blocking call on a worker holds the worker and every task it would run
([README: The rules](../README.md#the-rules), 1). A task writes `co_await m.scoped_lock_shared()`
([scoped_lock_shared](scoped_lock_shared.md)).

## Parameters

None.

## Return value

None.

## Complexity

Constant when no writer holds or waits: one atomic add. Otherwise a receive on the round's channel, made by the
first reader that waits.

## Exceptions

None. A receive of a channel may wake a waiting sender, whose wake may start the scheduler's workers, but nobody
sends on the channel of a round: the writer closes it.

## Notes

A reader that already holds the lock and takes it again while a writer waits waits for ever: the writer blocks the
readers that come after it, and waits for the first lock to be given back.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <shared_mutex>

using namespace sgcl;

int main() {
    async::shared_mutex m;
    vector<int> prices = {3, 4};  // guarded by m
    atomic<int> sum = 0;
    vector<thread> readers;
    for (int r : range(4)) {
        readers.emplace_back([&] {
            std::shared_lock lock(m);  // lock_shared(), unlock_shared() at the end of the scope
            sum += prices[0] + prices[1];
        });
    }
    for (auto& t : readers) {
        t.join();
    }
    println("{}", sum.load());
}
```

Output:

```text
28
```

## See also

- [try_lock_shared](try_lock_shared.md): the reader's lock without the wait
- [unlock_shared](unlock_shared.md): gives the reader's lock back
- [scoped_lock_shared](scoped_lock_shared.md): the reader's lock of a task
- [lock](lock.md): the writer's lock
- [sgcl::async::shared_mutex](README.md)
