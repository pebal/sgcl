[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex.md)

# sgcl::async::shared_mutex::scoped_lock

```cpp
auto scoped_lock() noexcept;
```

Locks the mutex for the writer for a scope: the writer's lock of a task, and of a thread that wants a
[guard](../shared_mutex-guard.md). The call does nothing yet; it returns an [operation](../operation.md), carried out
in one of two ways ([README: Waiting operations](../README.md#waiting-operations)):

- `auto guard = co_await m.scoped_lock();` in a task: the awaiter, a `shared_mutex::lock_op` in the task's frame,
  takes the lock at once when nobody holds it, allocating nothing; otherwise the task waits, holding no thread,
  first for the writers' mutex and then for the readers counted before it.
- `auto guard = m.scoped_lock().wait();` on a thread: blocks as [lock](lock.md) does.

Either way the guard holds the writer's lock and gives it back when it is destroyed.

## Parameters

None.

## Return value

An [operation](../operation.md). Carried out, by `co_await` or by `.wait()`, it gives a
[shared_mutex::guard](../shared_mutex-guard.md) of this mutex, locked for the writer.

## Complexity

Constant when nobody holds the mutex. A task that waits awaits a task made for the wait, one frame on the managed
heap.

## Exceptions

The call: none. Carried out: the receives of the wait, not `noexcept` for the reason given on
[lock](lock.md#exceptions); in a task, the fast path takes the writers' mutex, a receive too.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Registry {
    async::shared_mutex lock;
    vector<string> names;  // guarded by lock
};

async::task<> add(tracked_ptr<Registry> r, string name) {
    auto guard = co_await r->lock.scoped_lock();  // alone
    r->names.push_back(name);
}

int main() {
    tracked_ptr r = make_tracked<Registry>();
    async::spawn(add(r, "Ada")).wait();
    {
        auto guard = r->lock.scoped_lock().wait();  // a thread
        r->names.push_back("Grace");
    }
    println("{}", r->names.size());
}
```

Output:

```text
2
```

## See also

- [shared_mutex::guard](../shared_mutex-guard.md): what the operation gives
- [scoped_lock_shared](scoped_lock_shared.md): a reader's lock for a scope
- [lock](lock.md): the thread's lock, for the standard's guards
- [sgcl::async::shared_mutex](../shared_mutex.md)
