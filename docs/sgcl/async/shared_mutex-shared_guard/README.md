[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex/README.md)

# sgcl::async::shared_mutex::shared_guard

```cpp
#include "sgcl/async/shared_mutex.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class shared_mutex {
    public:
        class shared_guard;
    };
}
```

`sgcl::async::shared_mutex::shared_guard` holds a reader's lock of a [shared_mutex](../shared_mutex/README.md) for a scope
and gives it back when it is destroyed: what [scoped_lock_shared](../shared_mutex/scoped_lock_shared.md) gives,
`auto guard = co_await m.scoped_lock_shared();` in a task and `m.scoped_lock_shared().wait()` on a thread. It is
`std::shared_lock` for a task, which cannot hold a `std::shared_lock` across a `co_await` of the lock; on a thread
`std::shared_lock<sgcl::async::shared_mutex>` does the same. It can be moved, can give the lock up still held
([release](release.md)), and names the shared mutex it holds a lock of
([owner](owner.md)), as [mutex::guard](../mutex-guard/README.md) does.

The guard is one word, the address of the shared mutex: the mutex must outlive it.

## Rules

- A guard is an object of its scope: on a stack or in a task's frame, never in a managed object or a container, as
  any raw pointer ([The rules](../../core/README.md#the-rules), 3).
- Movable, not copyable, not assignable. A guard moved from or released holds nothing: its destructor does
  nothing, and its [owner](owner.md) and [release](release.md)
  give a null pointer. The shared mutex is given by its address, where a mutex's guard gives a handle: a shared
  mutex is an object, not a handle.
- The destructor gives the reader's lock back. As every destructor it is noexcept, and it throws nothing: a waiting
  writer whose wake ([unlock_shared](../shared_mutex/unlock_shared.md)) would have to start the scheduler's workers and
  cannot is queued all the same, and takes the lock when the workers next start.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](shared_mutex-shared_guard.md) | takes over a reader's lock, or another guard's |
| `(destructor)` | gives the reader's lock back, when the guard holds one |

#### Observers

| Function | Description |
|---|---|
| [owner](owner.md) | the shared mutex the guard holds a lock of |

#### Modifiers

| Function | Description |
|---|---|
| [release](release.md) | gives the lock up, still held |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Scores {
    async::shared_mutex lock;
    vector<int> values = {7, 9, 4};  // guarded by lock
};

async::task<int> best(tracked_ptr<Scores> s) {
    auto guard = co_await s->lock.scoped_lock_shared();
    int top = 0;
    for (int v : s->values) {
        top = v > top ? v : top;
    }
    co_return top;  // the guard gives the lock back here
}

int main() {
    tracked_ptr s = make_tracked<Scores>();
    println("{}", async::spawn(best(s)).wait());
    println("{}", s->lock.try_lock());
    s->lock.unlock();
}
```

Output:

```text
9
true
```

## See also

- [scoped_lock_shared](../shared_mutex/scoped_lock_shared.md): what makes a shared guard
- [shared_mutex::guard](../shared_mutex-guard/README.md): the writer's guard
- [mutex::guard](../mutex-guard/README.md): the guard of a mutex
- [sgcl::async::shared_mutex](../shared_mutex/README.md)
