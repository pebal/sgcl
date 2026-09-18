# SharedMutex

```cpp
#include "sgcl/Sgcl/Async/SharedMutex.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class SharedMutex;   // any number of readers, or one writer
}
```

The same class in the `sgcl` interface: [shared_mutex](../../async/shared_mutex.md).

Go's `sync.RWMutex` and Java's `ReentrantReadWriteLock`: any number of readers at once, or one writer, for tasks and threads alike. It is not a channel under a name, as the [Mutex](Mutex.md) and its family are: a reader takes the lock by adding one to a word and gives it back by subtracting one, so that readers, the common case, never meet a channel, and the channels are for the waits. The word holds the count of the readers holding or waiting and, in its low bit, whether a writer holds or waits; a writer sets the bit and waits for the readers counted before it to leave (the last of them sends it a signal on a channel of one), a reader that finds the bit set waits for that writer to leave (on a channel closed by the writer's unlock; the writer's generation, in the high half of the word, tells the reader its writer is gone even when the next one has taken its place). Writers are served one at a time through a [Mutex](Mutex.md). The preference is Go's: a writer waiting blocks the readers that arrive after it, so that a stream of readers cannot starve it, and the readers it held back get in before the next writer, because they are counted and the next writer waits for them, so that a stream of writers cannot starve them. Measured once (Apple M-series, release): a `LockShared` and `UnlockShared` pair on one task 13 ns, on four tasks at once 51 ns (the word bouncing between the workers); the channel-built `Mutex`'s `Lock` and `Unlock` pair 87 ns on one task, 393 ns on four contending (a receive and a send through the ring, and a waiter each time at four). The waits have the blocking form and the awaitable one, with a guard each (`SharedGuard`, `Guard`); no Select case, since a Select waits on channels and this lock is a word.

## Rules

- It lives where a `Ptr` may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); not copyable, not movable.
- Not recursive either way: a reader that takes the lock again while a writer waits deadlocks (the writer blocks new readers, Go's rule), a writer that takes it again deadlocks on itself.
- `std::shared_lock<SharedMutex>` and `std::lock_guard<SharedMutex>` work for a thread (the class has the lowercase names for them); `co_await m.AsyncScopedLockShared()` and `co_await m.AsyncScopedLock()` for a task. Up to 2<sup>31</sup> readers at once.

## Members

```cpp
void LockShared();  bool TryLockShared() noexcept;  void UnlockShared();   // a reader
void Lock();  bool TryLock();  void Unlock();                              // the writer
auto AsyncLockShared() noexcept;               // co_await: a reader, locked
auto AsyncScopedLockShared() noexcept;         // co_await: a SharedGuard that unlocks when destroyed
auto AsyncLock() noexcept;                     // co_await: the writer, locked
auto AsyncScopedLock() noexcept;               // co_await: a Guard
using SharedGuard = sgcl::shared_mutex::shared_guard;  using Guard = sgcl::shared_mutex::guard;
void lock_shared();  bool try_lock_shared() noexcept;  void unlock_shared();   // for std::shared_lock<SharedMutex>
void lock();  bool try_lock();  void unlock();                                  // for std::lock_guard<SharedMutex>
```

```cpp
SharedMutex table;
auto read = [](SharedMutex& table) -> Task<> {
    auto guard = co_await table.AsyncScopedLockShared();   // with the other readers
};
auto write = [](SharedMutex& table) -> Task<> {
    auto guard = co_await table.AsyncScopedLock();         // alone
};
```

## Example

A table read by four tasks and grown by one, the readers' results handed to the main thread through a queue under a [Mutex](Mutex.md) with a [ConditionVariable](ConditionVariable.md).

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>
#include <mutex>

// A table read by many tasks and grown by one, under a SharedMutex; the
// readers' results handed to the main thread through a queue under a
// Mutex with a ConditionVariable. Every wait of a task is a co_await.
struct Table {
    SharedMutex lock;
    List<int> squares;           // guarded by lock
};

struct Results {
    Mutex lock;
    ConditionVariable ready;
    List<long> queue;            // guarded by lock
    int pending = 0;             // guarded by lock
};

Task<> Reader(Ptr<Table> table, Ptr<Results> results, int rounds) {
    long consistent = 0;
    for (int round : Range(rounds)) {
        (void)round;
        auto guard = co_await table->lock.AsyncScopedLockShared();   // any number of readers at once, never with the writer
        bool ok = true;
        for (size_t i : Range(table->squares.Count())) {
            ok = ok && table->squares[i] == (int)((i + 1) * (i + 1));
        }
        consistent += ok;
        co_await Yield();
    }
    auto guard = co_await results->lock.AsyncScopedLock();
    results->queue.Add(consistent);
    --results->pending;
    results->ready.NotifyOne();
}

Task<> Writer(Ptr<Table> table, int rounds) {
    for (int i : Range(rounds)) {
        auto guard = co_await table->lock.AsyncScopedLock();         // alone: the readers wait, and new ones queue behind it
        table->squares.Add((i + 1) * (i + 1));
        co_await Yield();
    }
}

int main() {
    Ptr table = Make<Table>();
    Ptr results = Make<Results>();
    results->pending = 4;
    for (int i : Range(4)) {
        (void)i;
        Go(Reader(table, results, 100));
    }
    Task<> w = Spawn(Writer(table, 100));
    long total = 0;
    std::unique_lock lock(results->lock);                                  // this thread: the standard's lock over the module's Mutex
    results->ready.Wait(lock, [&] { return results->pending == 0; });     // the predicate, checked under the mutex before each wait
    for (long consistent : results->queue) {
        total += consistent;
    }
    lock.unlock();
    w.Join();
    std::cout << table->squares.Count() << " squares, the last " << table->squares.Last() << "\n";
    std::cout << results->queue.Count() << " readers, " << total << " consistent looks at the table\n";
    Scheduler::Stop();
}
```

The output:

```
100 squares, the last 10000
4 readers, 400 consistent looks at the table
```

## See also

- [Mutex](Mutex.md): one holder at a time, what the writers queue on; [ConditionVariable](ConditionVariable.md): a wait under the mutex; [CopyOnWrite](../Concurrent/CopyOnWrite.md): a value read by many and replaced whole, without a lock; [Channel](Channel.md): the waits
- `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.
