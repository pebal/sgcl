[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](README.md)

# sgcl::async::shared_mutex::scoped_lock_shared

```cpp
auto scoped_lock_shared() noexcept;
```

Locks the mutex for a reader for a scope: the reader's lock of a task, and of a thread that wants a
[shared_guard](../shared_mutex-shared_guard/README.md). The call does nothing yet; it returns an
[operation](../operation/README.md), carried out in one of two ways
([README: Waiting operations](../README.md#waiting-operations)):

- `auto guard = co_await m.scoped_lock_shared();` in a task: the awaiter, a `shared_mutex::lock_op` in the task's
  frame, counts the reader in and goes on at once when no writer holds or waits, allocating nothing; otherwise the
  task waits for that writer to leave, holding no thread.
- `auto guard = m.scoped_lock_shared().wait();` on a thread: blocks as [lock_shared](lock_shared.md) does.

Either way the guard holds a reader's lock and gives it back when it is destroyed.

## Parameters

None.

## Return value

An [operation](../operation/README.md). Carried out, by `co_await` or by `.wait()`, it gives a
[shared_mutex::shared_guard](../shared_mutex-shared_guard/README.md) of this mutex, locked for a reader.

## Complexity

Constant when no writer holds or waits: one atomic add. A task that waits awaits a task made for the wait, one
frame on the managed heap.

## Exceptions

The call: none. Carried out: the receive of the wait, not `noexcept` for the reason given on
[lock_shared](lock_shared.md#exceptions); the fast path throws nothing.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Config {
    async::shared_mutex lock;
    string name = "prod";  // guarded by lock
};

async::task<string> read_name(tracked_ptr<Config> c) {
    auto guard = co_await c->lock.scoped_lock_shared();  // with the other readers
    co_return c->name;
}

int main() {
    tracked_ptr c = make_tracked<Config>();
    vector<async::task<string>> readers;
    for (int i : range(3)) {
        readers.push_back(async::spawn(read_name(c)));
    }
    for (auto& r : readers) {
        println("{}", r.wait());
    }
    auto guard = c->lock.scoped_lock_shared().wait();  // a thread
    println("{}", c->name);
}
```

Output:

```text
prod
prod
prod
prod
```

## See also

- [shared_mutex::shared_guard](../shared_mutex-shared_guard/README.md): what the operation gives
- [scoped_lock](scoped_lock.md): the writer's lock for a scope
- [lock_shared](lock_shared.md): the thread's reader lock, for `std::shared_lock`
- [sgcl::async::shared_mutex](README.md)
