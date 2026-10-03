[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](README.md)

# sgcl::async::shared_mutex::lock

```cpp
void lock() noexcept;
```

Locks the mutex for the writer, blocking the calling thread until it holds it alone. The writer takes the writers'
mutex first, so writers come in one at a time; then it sets its bit in the word, which holds back every reader that
comes after it, and waits for the readers counted before the bit to leave, the last of whom signals it. The
standard's Lockable member, for `std::lock_guard<sgcl::async::shared_mutex>` and `std::unique_lock` on a thread.

A task does not call it: a blocking call on a worker holds the worker and every task it would run
([README: The rules](../README.md#the-rules), 1). A task writes `co_await m.scoped_lock()`
([scoped_lock](scoped_lock.md)).

## Parameters

None.

## Return value

None.

## Complexity

Constant when nobody holds the mutex: a lock of the writers' mutex and an atomic add on the word. Otherwise a
receive on the writers' mutex while another writer holds it, then one on the channel the last reader signals.

## Exceptions

None. A receive of a channel may wake a waiting sender, whose wake may start the scheduler's workers, but nobody
waits to send on the channels a writer receives from.

## Notes

Not recursive: a writer that takes the lock again waits for itself for ever.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <mutex>

using namespace sgcl;

int main() {
    async::shared_mutex m;
    int counter = 0;  // guarded by m
    vector<thread> writers;
    for (int t : range(4)) {
        writers.emplace_back([&] {
            for (int i : range(1000)) {
                std::lock_guard guard(m);  // lock(), unlock() at the end of the scope
                ++counter;
            }
        });
    }
    for (auto& t : writers) {
        t.join();
    }
    println("{}", counter);
}
```

Output:

```text
4000
```

## See also

- [try_lock](try_lock.md): the writer's lock without the wait
- [unlock](unlock.md): gives the writer's lock back
- [scoped_lock](scoped_lock.md): the writer's lock of a task
- [lock_shared](lock_shared.md): a reader's lock
- [sgcl::async::shared_mutex](README.md)
