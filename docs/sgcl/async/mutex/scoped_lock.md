[sgcl](../../README.md) › [async](../README.md) › [mutex](README.md)

# sgcl::async::mutex::scoped_lock

```cpp
auto scoped_lock() const noexcept;
```

Locks the mutex for a scope: the lock of a task, and of a thread that wants a [guard](../mutex-guard/README.md). The call
does nothing yet; it returns an [operation](../operation/README.md), carried out in one of two ways
([README: Waiting operations](../README.md#waiting-operations)):

- `auto guard = co_await m.scoped_lock();` in a task: the task suspends while the mutex is locked, holding no
  thread, and is resumed by the [unlock](unlock.md) that hands it the mutex. The awaiter is a
  `mutex::scoped_lock_op` in the task's frame.
- `auto guard = m.scoped_lock().wait();` on a thread: blocks as [lock](lock.md) does.

Either way the guard holds the mutex locked and unlocks it when it is destroyed.

## Parameters

None.

## Return value

An [operation](../operation/README.md). Carried out, by `co_await` or by `.wait()`, it gives a
[mutex::guard](../mutex-guard/README.md) of this mutex, locked.

## Complexity

Constant when the mutex is free: a receive through the channel's ring. Otherwise a waiter is registered on the
channel's list until an unlock hands the mutex over.

## Exceptions

The call: none. Carried out: the receive of the channel's signal, not `noexcept` for the reason given on
[lock](lock.md#exceptions).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Log {
    async::mutex lock;
    vector<string> lines;  // guarded by lock
};

async::task<> append(tracked_ptr<Log> log, string line) {
    auto guard = co_await log->lock.scoped_lock();
    log->lines.push_back(line);
}

int main() {
    tracked_ptr log = make_tracked<Log>();
    async::task<> a = async::spawn(append(log, "from a task"));
    a.wait();
    {
        auto guard = log->lock.scoped_lock().wait();  // a thread
        log->lines.push_back("from main");
    }
    for (auto& line : log->lines) {
        println("{}", line);
    }
}
```

Output:

```text
from a task
from main
```

## See also

- [mutex::guard](../mutex-guard/README.md): what the operation gives
- [lock](lock.md): the thread's lock, for the standard's guards
- [on_lock](on_lock.md): the lock as a case of a select
- [condition_variable::wait](../condition_variable/wait.md): a wait with the guard
- [sgcl::async::mutex](README.md)
