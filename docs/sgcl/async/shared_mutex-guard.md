[sgcl](../README.md) › [async](README.md) › [shared_mutex](shared_mutex.md)

# sgcl::async::shared_mutex::guard

```cpp
#include "sgcl/async/shared_mutex.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class shared_mutex {
    public:
        class guard;
    };
}
```

`sgcl::async::shared_mutex::guard` holds the writer's lock of a [shared_mutex](shared_mutex.md) for a scope and
gives it back when it is destroyed: what [scoped_lock](shared_mutex/scoped_lock.md) gives,
`auto guard = co_await m.scoped_lock();` in a task and `m.scoped_lock().wait()` on a thread. It is `std::lock_guard`
for a task, which cannot hold a `std::lock_guard` across a `co_await` of the lock; on a thread
`std::lock_guard<sgcl::async::shared_mutex>` does the same. It can be moved, can give the lock up still held
([release](shared_mutex-guard/release.md)), and names the shared mutex it holds the lock of
([owner](shared_mutex-guard/owner.md)), as [mutex::guard](mutex-guard.md) does.

The guard is one word, the address of the shared mutex: the mutex must outlive it.

## Rules

- A guard is an object of its scope: on a stack or in a task's frame, never in a managed object or a container, as
  any raw pointer ([The rules](../core/README.md#the-rules), 3).
- Movable, not copyable, not assignable. A guard moved from or released holds nothing: its destructor does
  nothing, and its [owner](shared_mutex-guard/owner.md) and [release](shared_mutex-guard/release.md) give a null
  pointer. The shared mutex is given by its address, where a mutex's guard gives a handle: a shared mutex is an
  object, not a handle.
- The destructor gives the writer's lock back. As every destructor it is noexcept, and it throws nothing: the
  readers held back and the next writer ([unlock](shared_mutex/unlock.md)) are all woken, a task whose wake would
  have to start the scheduler's workers and cannot queued all the same, to run when the workers next start.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](shared_mutex-guard/shared_mutex-guard.md) | takes over the writer's lock, or another guard's |
| `(destructor)` | gives the writer's lock back, when the guard holds it |

#### Observers

| Function | Description |
|---|---|
| [owner](shared_mutex-guard/owner.md) | the shared mutex the guard holds the lock of |

#### Modifiers

| Function | Description |
|---|---|
| [release](shared_mutex-guard/release.md) | gives the lock up, still held |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Stock {
    async::shared_mutex lock;
    int items = 5;  // guarded by lock
};

async::task<bool> take(tracked_ptr<Stock> s, int n) {
    auto guard = co_await s->lock.scoped_lock();
    if (s->items < n) {
        co_return false;  // the guard gives the lock back on this path too
    }
    s->items -= n;
    co_return true;
}

int main() {
    tracked_ptr s = make_tracked<Stock>();
    for (int n : {3, 3, 2}) {
        println("{}: {}", n, async::spawn(take(s, n)).wait());
    }
    println("{} left, free: {}", s->items, s->lock.try_lock_shared());
    s->lock.unlock_shared();
}
```

Output:

```text
3: true
3: false
2: true
0 left, free: true
```

## See also

- [scoped_lock](shared_mutex/scoped_lock.md): what makes a guard
- [shared_mutex::shared_guard](shared_mutex-shared_guard.md): a reader's guard
- [mutex::guard](mutex-guard.md): the guard of a mutex
- [sgcl::async::shared_mutex](shared_mutex.md)
