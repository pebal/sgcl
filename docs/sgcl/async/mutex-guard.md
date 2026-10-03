[sgcl](../README.md) › [async](README.md) › [mutex](mutex.md)

# sgcl::async::mutex::guard

```cpp
#include "sgcl/async/mutex.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class mutex {
    public:
        class guard;
    };
}
```

`sgcl::async::mutex::guard` holds a [mutex](mutex.md) locked for a scope and unlocks it when it is destroyed: what
[scoped_lock](mutex/scoped_lock.md) gives, `auto guard = co_await m.scoped_lock();` in a task and
`m.scoped_lock().wait()` on a thread. It is `std::lock_guard` for a task, which cannot hold a `std::lock_guard`
across a `co_await` of the lock; on a thread `std::lock_guard<sgcl::async::mutex>` does the same. Like
`std::unique_lock`, it can be moved and can give the mutex up still locked ([release](mutex-guard/release.md)), and
it names the mutex it holds ([owner](mutex-guard/owner.md)), which a
[condition_variable](condition_variable.md) lets go of and takes back around its wait.

The guard is one word, the address of the mutex's state, not a tracked word: on a stack or in a task's frame, both
scanned conservatively, that word keeps the state alive.

## Rules

- A guard is an object of its scope: on a stack or in a task's frame, never in a managed object or a container, as
  any raw pointer ([The rules](../core/README.md#the-rules), 3): a store there has no barrier, and may land in an
  object the cycle has scanned already.
- Movable, not copyable, not assignable. A guard moved from or released holds nothing: its destructor does
  nothing, and its [owner](mutex-guard/owner.md) and [release](mutex-guard/release.md) give `nullopt`.
- The destructor unlocks the mutex. As every destructor it is noexcept, and it throws nothing: a waiting task whose
  wake ([unlock](mutex/unlock.md)) would have to start the scheduler's workers and cannot is queued all the same,
  and takes the mutex when the workers next start.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](mutex-guard/mutex-guard.md) | takes over a locked mutex, or another guard's |
| `(destructor)` | unlocks the mutex, when the guard holds one |

#### Observers

| Function | Description |
|---|---|
| [owner](mutex-guard/owner.md) | the mutex the guard holds |

#### Modifiers

| Function | Description |
|---|---|
| [release](mutex-guard/release.md) | gives the mutex up, still locked |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Account {
    async::mutex lock;
    int balance = 100;  // guarded by lock
};

async::task<bool> withdraw(tracked_ptr<Account> a, int amount) {
    auto guard = co_await a->lock.scoped_lock();
    if (a->balance < amount) {
        co_return false;  // the guard unlocks on this path too
    }
    a->balance -= amount;
    co_return true;
}

int main() {
    tracked_ptr a = make_tracked<Account>();
    for (int amount : {30, 50, 40}) {
        async::task<bool> t = async::spawn(withdraw(a, amount));
        println("{}: {}", amount, t.wait());
    }
    println("{} left, free: {}", a->balance, a->lock.try_lock());
    a->lock.unlock();
}
```

Output:

```text
30: true
50: true
40: false
20 left, free: true
```

## See also

- [scoped_lock](mutex/scoped_lock.md): what makes a guard
- [condition_variable::wait](condition_variable/wait.md): a wait with the guard
- [shared_mutex::guard](shared_mutex-guard.md), [shared_mutex::shared_guard](shared_mutex-shared_guard.md): the
  guards of a shared mutex
- [sgcl::async::mutex](mutex.md)
