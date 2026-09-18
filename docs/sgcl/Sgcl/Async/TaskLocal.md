# TaskLocal

```cpp
#include "sgcl/Sgcl/Async/TaskLocal.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class TaskLocal;   // a value visible to a task and to the tasks it starts
}
```

The same in the `sgcl` interface: [task_local](../../async/task_local.md).

A value visible to a task and to the tasks it starts, read without passing it through every signature: a request id, a deadline, a stop token, a logger, the current user. Go's `context.WithValue`, Kotlin's `CoroutineContext`, tokio's `task_local!`. A `TaskLocal<T>` is declared once, at namespace scope, and the variable is the key: `TaskLocal<int> RequestId;`. `co_await RequestId.Set(7)` sets the value for the task from that line on, and for every task it starts from then on; `RequestId.Get()` reads it, from the coroutine's body or from any function it calls, `None` when unset and outside a task; `RequestId.With(7, t)` is a task that runs `t` with the value set.

The values live in managed nodes, one per `Set`, chained newest first, and the head of a task's chain is a word at the front of its frame, next to the executor of [Executor](Executor.md); the variable itself holds nothing and lives anywhere. A task started by another (`Spawn`, `Go`, `co_await` of a task nobody started, an executor's `Spawn`) takes the head of the starting task's chain as it is at that moment: inheritance is one word copied. A node is never changed once made, so a child that sets a value puts a node of its own in front of the shared tail and the parent and the siblings keep what they had. `Get()` works from a plain function because the thread knows which frame it runs.

## Rules

- A `TaskLocal<T>` is a static, one per key; it is not copyable. Its values are in managed nodes: `T` may hold a `Ptr`.
- `Set` is `co_await`ed: the `co_await` is what names the coroutine whose value it is. It never suspends.
- What a task sets after starting a child is the task's own; what a child sets is the child's. Nothing a task sets is seen by its parent or its siblings.
- `Get()` outside a task is `None`; `GetOr(fallback)` is the fallback then.
- A handful of keys is what this is for: a miss walks the task's whole chain.

## Members

```cpp
TaskLocal() noexcept;

auto Set(T value);                // co_await x.Set(v): the value from here on, for this task and the tasks it starts from here on
Optional<T> Get() const;          // the value of the task this thread runs; None when unset, and outside a task
T GetOr(T fallback) const;        // the value, or the fallback
bool IsSet() const noexcept;
template<class U> Task<U> With(T value, Task<U> t);   // a task that runs t with the value set
sgcl::task_local<T>& Inner() noexcept;
```

```cpp
TaskLocal<Ptr<User>> CurrentUser;

Task<> Handle(Ptr<Request> r) {
    co_await CurrentUser.Set(r->user);           // for this task and the ones it starts
    co_await Validate(r);                        // sees the user, and so does what Validate calls
    Go(Audit(r));                                // a detached task: inherits it too
    co_await CurrentUser.With(admin, Repair(r)); // Repair runs as admin; this task stays r->user
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A request id and a user, set once by the handler and read by every
// function and task under it: the logger takes no arguments for them.
using namespace std::chrono_literals;

TaskLocal<int> RequestId;
TaskLocal<String> User;

void Log(const char* what) {                        // a plain function: the values of the task that called it
    std::cout << "[request " << RequestId.GetOr(0) << ", " << User.GetOr("nobody") << "] " << what << "\n";
}

Task<> Store(int value) {
    Log("storing");
    co_await Sleep(1ms);                            // the values survive a wait
    std::cout << "  value " << value << "\n";
    Log("stored");
}

Task<> Audit() {
    co_await User.Set("auditor");                   // this task's own: the handler keeps its user
    Log("audited");
}

Task<> Handle(int id, String who, int value) {
    co_await RequestId.Set(id);
    co_await User.Set(who);
    Log("handling");
    co_await Store(value);                          // an awaited task inherits both
    co_await Spawn(Audit());                        // a spawned one too, and sets one of its own
    co_await User.With("guest", Store(value + 1));  // Store runs as guest
    Log("done");
}

int main() {
    Spawn(Handle(1, "ann", 10)).Join();
    Spawn(Handle(2, "bob", 20)).Join();
    Log("outside a task");
    Sgcl::Scheduler::Stop();
    return 0;
}
```

The output:

```
[request 1, ann] handling
[request 1, ann] storing
  value 10
[request 1, ann] stored
[request 1, auditor] audited
[request 1, guest] storing
  value 11
[request 1, guest] stored
[request 1, ann] done
[request 2, bob] handling
[request 2, bob] storing
  value 20
[request 2, bob] stored
[request 2, auditor] audited
[request 2, guest] storing
  value 21
[request 2, guest] stored
[request 2, bob] done
[request 0, nobody] outside a task
```

## See also

- [Executor](Executor.md): where a task runs, remembered in the same header of the frame; [StopToken](StopToken.md): a token is one thing to keep in a task-local
- [Task](Task.md): the tasks, the frames on the managed heap; [Scheduler](Scheduler.md): `Spawn`, `Go`
- [README: Coroutines](../../async/README.md#coroutines)
- `tests/Sgcl/executor.cpp`: the facade, checked; `tests/async/task_local.cpp`: every behaviour.
