# sgcl::shared_mutex

```cpp
#include "sgcl/async/shared_mutex.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class shared_mutex;   // any number of readers, or one writer
}
```

Go's `sync.RWMutex` and Java's `ReentrantReadWriteLock`: any number of readers at once, or one writer, for tasks and threads alike. It is not a channel under a name, as the [mutex](mutex.md) and its family are: a reader takes the lock by adding one to a word and gives it back by subtracting one, so that readers, the common case, never meet a channel, and the channels are for the waits. The word holds the count of the readers holding or waiting and, in its low bit, whether a writer holds or waits; a writer sets the bit and waits for the readers counted before it to leave (the last of them sends it a signal on a channel of one), a reader that finds the bit set waits for that writer to leave (on a channel closed by the writer's unlock; the writer's generation, in the high half of the word, tells the reader its writer is gone even when the next one has taken its place). Writers are served one at a time through a [mutex](mutex.md). The preference is Go's: a writer waiting blocks the readers that arrive after it, so that a stream of readers cannot starve it, and the readers it held back get in before the next writer, because they are counted and the next writer waits for them, so that a stream of writers cannot starve them. Measured once (Apple M-series, release): a `lock_shared` and `unlock_shared` pair on one task 13 ns, on four tasks at once 51 ns (the word bouncing between the workers); the channel-built `mutex`'s `lock` and `unlock` pair 87 ns on one task, 393 ns on four contending (a receive and a send through the ring, and a waiter each time at four). The waits have the blocking form and the awaitable one, with a guard each (`shared_guard`, `guard`); no select case, since a select waits on channels and this lock is a word.

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- Not recursive either way: a reader that takes the lock again while a writer waits deadlocks (the writer blocks new readers, Go's rule), a writer that takes it again deadlocks on itself.
- `std::shared_lock<sgcl::shared_mutex>` and `std::lock_guard<sgcl::shared_mutex>` work for a thread; `co_await m.async_scoped_lock_shared()` and `co_await m.async_scoped_lock()` for a task. Up to 2<sup>31</sup> readers at once.

## Members

```cpp
void lock_shared();  bool try_lock_shared() noexcept;  void unlock_shared();   // a reader
void lock();  bool try_lock();  void unlock();                                  // the writer
auto async_lock_shared() noexcept;             // co_await: a reader, locked
auto async_scoped_lock_shared() noexcept;      // co_await: a shared_guard that unlocks when destroyed
auto async_lock() noexcept;                    // co_await: the writer, locked
auto async_scoped_lock() noexcept;             // co_await: a guard
class shared_guard;  class guard;
```

```cpp
sgcl::shared_mutex table;
auto read = [](sgcl::shared_mutex& table) -> sgcl::task<> {
    auto guard = co_await table.async_scoped_lock_shared();   // with the other readers
};
auto write = [](sgcl::shared_mutex& table) -> sgcl::task<> {
    auto guard = co_await table.async_scoped_lock();          // alone
};
```

## Example

A table read by four tasks and grown by one, the readers' results handed to the main thread through a queue under a [mutex](mutex.md) with a [condition_variable](condition_variable.md).

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <mutex>

// A table read by many tasks and grown by one, under a shared_mutex; the
// readers' results handed to the main thread through a queue under a
// mutex with a condition variable. Every wait of a task is a co_await.
struct Table {
    sgcl::shared_mutex lock;
    sgcl::vector<int> squares;   // guarded by lock
};

struct Results {
    sgcl::mutex lock;
    sgcl::condition_variable ready;
    sgcl::vector<long> queue;    // guarded by lock
    int pending = 0;             // guarded by lock
};

sgcl::task<> reader(sgcl::tracked_ptr<Table> table, sgcl::tracked_ptr<Results> results, int rounds) {
    long consistent = 0;
    for (int round : sgcl::range(rounds)) {
        (void)round;
        auto guard = co_await table->lock.async_scoped_lock_shared();   // any number of readers at once, never with the writer
        bool ok = true;
        for (size_t i : sgcl::range(table->squares.size())) {
            ok = ok && table->squares[i] == (int)((i + 1) * (i + 1));
        }
        consistent += ok;
        co_await sgcl::yield();
    }
    auto guard = co_await results->lock.async_scoped_lock();
    results->queue.push_back(consistent);
    --results->pending;
    results->ready.notify_one();
}

sgcl::task<> writer(sgcl::tracked_ptr<Table> table, int rounds) {
    for (int i : sgcl::range(rounds)) {
        auto guard = co_await table->lock.async_scoped_lock();         // alone: the readers wait, and new ones queue behind it
        table->squares.push_back((i + 1) * (i + 1));
        co_await sgcl::yield();
    }
}

int main() {
    sgcl::tracked_ptr table = sgcl::make_tracked<Table>();
    sgcl::tracked_ptr results = sgcl::make_tracked<Results>();
    results->pending = 4;
    for (int i : sgcl::range(4)) {
        (void)i;
        sgcl::go(reader(table, results, 100));
    }
    sgcl::task<> w = sgcl::spawn(writer(table, 100));
    long total = 0;
    std::unique_lock lock(results->lock);                                  // this thread: the standard's lock over the module's mutex
    results->ready.wait(lock, [&] { return results->pending == 0; });     // the predicate, checked under the mutex before each wait
    for (long consistent : results->queue) {
        total += consistent;
    }
    lock.unlock();
    w.join();
    std::cout << table->squares.size() << " squares, the last " << table->squares.back() << "\n";
    std::cout << results->queue.size() << " readers, " << total << " consistent looks at the table\n";
    sgcl::scheduler::stop();
}
```

The output:

```
100 squares, the last 10000
4 readers, 400 consistent looks at the table
```

## See also

- [mutex](mutex.md): one holder at a time, what the writers queue on; [condition_variable](condition_variable.md): a wait under the mutex; [copy_on_write](../concurrent/copy_on_write.md): a value read by many and replaced whole, without a lock; [channel](channel.md): the waits
- `tests/async/shared_mutex.cpp`: every behaviour above, checked.
