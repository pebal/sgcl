[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::shared_mutex

```cpp
#include "sgcl/async/shared_mutex.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class shared_mutex {
    public:
        class shared_guard;
        class guard;

        template<bool Shared, bool Scoped>
        class lock_op;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::shared_mutex` lets any number of readers in at once, or one writer, for tasks and threads alike: Go's
`sync.RWMutex`, Java's `ReentrantReadWriteLock`, `std::shared_mutex` with a wait that holds no thread. It is not a
channel under a name, as the [mutex](../mutex/README.md) and its family are: a reader takes the lock by adding one to a word
and gives it back by subtracting one, so that readers, the common case, never meet a channel; the channels are for
the waits.

The word holds the count of the readers that hold or wait for the lock and, in its low bit, whether a writer holds
or waits for it. A writer sets the bit and waits for the readers counted before it to leave; the last of them sends
it a signal on a channel of one. A reader that finds the bit set waits for that writer to leave, on a channel the
writer's unlock closes; the writer's generation, in the high half of the word, tells such a reader that its writer
is gone even when the next one has taken its place, and that next writer counted the reader and waits for it.
Writers are served one at a time through a mutex of the module.

The preference is Go's: a writer waiting blocks the readers that arrive after it, so that a stream of readers cannot
starve it, and the readers it held back get in before the next writer, because they are counted and the next writer
waits for them, so that a stream of writers cannot starve them.

## Rules

- A shared mutex is an object, not a handle: it is neither copied nor moved. Tasks reach it through the object that
  holds it.
- Not recursive either way: a reader that takes the lock again while a writer waits waits for ever (the writer
  blocks new readers, Go's rule), and a writer that takes it again waits for itself.
- A thread locks with the standard's Lockable members, through `std::shared_lock<sgcl::async::shared_mutex>` for a
  reader and `std::lock_guard` or `std::unique_lock` for the writer; a task locks with
  `co_await m.scoped_lock_shared()` and `co_await m.scoped_lock()`, which hold no thread while they wait.
- Fewer than 2^31 readers hold or wait for the lock at once: the count has 31 bits.
- The lock is a word, not a channel, so it has no case of a [select](../select.md).

## Member types

| Type | Definition |
|---|---|
| `shared_guard` | a reader's lock held for a scope ([shared_guard](../shared_mutex-shared_guard/README.md)) |
| `guard` | the writer's lock held for a scope ([guard](../shared_mutex-guard/README.md)) |
| `lock_op<Shared, Scoped>` | the awaiter that `co_await` of [scoped_lock](scoped_lock.md) and [scoped_lock_shared](scoped_lock_shared.md) makes in a task |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](shared_mutex.md) | constructs an unlocked shared mutex |
| `(destructor)` | destroys the shared mutex, which must not be locked |

#### Exclusive locking

| Function | Description |
|---|---|
| [lock](lock.md) | locks the mutex for the writer, blocking the thread |
| [try_lock](try_lock.md) | locks the mutex for the writer when nobody holds it, without waiting |
| [unlock](unlock.md) | unlocks the writer's lock |
| [scoped_lock](scoped_lock.md) | locks the mutex for the writer for a scope, waiting in a task or on a thread |

#### Shared locking

| Function | Description |
|---|---|
| [lock_shared](lock_shared.md) | locks the mutex for a reader, blocking the thread |
| [try_lock_shared](try_lock_shared.md) | locks the mutex for a reader when no writer holds or waits, without waiting |
| [unlock_shared](unlock_shared.md) | unlocks a reader's lock |
| [scoped_lock_shared](scoped_lock_shared.md) | locks the mutex for a reader for a scope, waiting in a task or on a thread |

## Complexity

A reader's lock and unlock are one atomic add and one atomic subtract on the word, a few times less than the
channel-built [mutex](../mutex/README.md)'s receive and send through its ring; an awaited lock that does not wait allocates
nothing. A writer takes the writers' mutex and sets the bit; a wait, a reader's or the writer's, is a channel's
receive, and per round of readers held back by a writer one channel of signals is made.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Table {
    async::shared_mutex lock;
    vector<int> squares;  // guarded by lock
};

async::task<int> reader(tracked_ptr<Table> table) {
    int consistent = 0;
    for (int round : range(100)) {
        auto guard = co_await table->lock.scoped_lock_shared();  // with the other readers
        bool ok = true;
        for (size_t i : range(table->squares.size())) {
            ok = ok && table->squares[i] == int((i + 1) * (i + 1));
        }
        consistent += ok;
    }
    co_return consistent;
}

async::task<> writer(tracked_ptr<Table> table) {
    for (int i : range(1, 101)) {
        auto guard = co_await table->lock.scoped_lock();  // alone
        table->squares.push_back(i * i);
    }
}

int main() {
    tracked_ptr table = make_tracked<Table>();
    async::task<> w = async::spawn(writer(table));
    vector<async::task<int>> readers;
    for (int r : range(4)) {
        readers.push_back(async::spawn(reader(table)));
    }
    int total = 0;
    for (auto& r : readers) {
        total += r.wait();
    }
    w.wait();
    println("{} squares, the last {}", table->squares.size(), table->squares.back());
    println("{} consistent looks of 400", total);
}
```

Output:

```text
100 squares, the last 10000
400 consistent looks of 400
```

## See also

- [shared_mutex::shared_guard](../shared_mutex-shared_guard/README.md), [shared_mutex::guard](../shared_mutex-guard/README.md): the
  locks held for a scope
- [mutex](../mutex/README.md): one holder at a time, what the writers queue on
- [condition_variable](../condition_variable/README.md): a wait under a mutex
- [copy_on_write](../../concurrent/copy_on_write/README.md): a value read by many and replaced whole, without a lock
- [channel](../channel/README.md): the waits
