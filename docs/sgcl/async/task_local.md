# sgcl::task_local

```cpp
#include "sgcl/async/task_local.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class task_local;   // a value visible to a task and to the tasks it starts
}
```

A value visible to a task and to the tasks it starts, read without passing it through every signature: a request id, a deadline, a stop token, a logger, the current user. Go's `context.WithValue`, Kotlin's `CoroutineContext`, tokio's `task_local!`. A `task_local<T>` is declared once, at namespace scope, and the variable is the key: `sgcl::task_local<int> request_id;`. `co_await request_id.set(7)` sets the value for the task from that line on, and for every task it starts from then on; `request_id.get()` reads it, from the coroutine's body or from any function it calls, however deep, `nullopt` when unset and outside a task; `request_id.with(7, t)` is a task that runs `t` with the value set.

The values live in managed nodes, one per `sorted_set`, chained newest first, and the head of a task's chain is a word at the front of its frame (`detail::FrameHeader`, next to the executor of [executor](executor.md)); the variable itself holds nothing and lives anywhere. A task started by another (`spawn`, `go`, `co_await` of a task nobody started, an executor's `spawn`) takes the head of the starting task's chain as it is at that moment: inheritance is one word copied. A node is never changed once made, so a child that sets a value puts a node of its own in front of the shared tail and the parent and the siblings keep what they had, copy on write by construction; a task that sets the same key twice reads the newest. The chain lives while a frame points at it; a value is copied out by `get()`: a `tracked_ptr`, a string, a token, an int, whatever is cheap to copy and safe to share between the tasks that inherit it.

`get()` works from a plain function because the thread knows which frame it runs: the scheduler, an executor and `task::resume()` set a thread-local to the frame around every resume (`detail::current_frame`). The same thread-local is what a start reads to inherit from, so a task started from a plain thread, outside any task, inherits nothing.

## Rules

- A `task_local<T>` is a static, one per key; it is not copyable. Its values are in managed nodes: `T` may hold a `tracked_ptr` and lives as a member of a managed object does.
- `sorted_set` is `co_await`ed: the `co_await` is what names the coroutine whose value it is. It never suspends. A function the task calls reads; the task sets.
- What a task sets after starting a child is the task's own; what a child sets is the child's. Nothing a task sets is seen by its parent or its siblings.
- `get()` outside a task (a plain thread, a worker between two tasks) is `nullopt`; `get_or(fallback)` is the fallback then.
- The chain is read by `get()` from the front: a task that sets many keys, or sets one key many times, walks them all for a miss. A handful of keys is what this is for.

## Members

```cpp
task_local() noexcept;

setter set(T value);              // co_await x.set(v): the value from here on, for this task and the tasks it starts from here on
optional<T> get() const;          // the value of the task this thread runs; nullopt when unset, and outside a task
T get_or(T fallback) const;       // the value, or the fallback
bool is_set() const noexcept;
template<class U> task<U> with(T value, task<U> t);   // a task that runs t with the value set
```

`with(v, t)` is a task of its own, the wrapper: it sets the value, awaits `t`, which inherits it, and returns what `t` returned; the task that awaits or spawns the wrapper keeps its own value.

```cpp
task_local<tracked_ptr<User>> current_user;

task<> handle(tracked_ptr<Request> r) {
    co_await current_user.set(r->user);           // for this task and the ones it starts
    co_await validate(r);                         // sees the user, and so does what validate calls
    go(audit(r));                           // a detached task: inherits it too
    co_await current_user.with(admin, repair(r)); // repair runs as admin; this task stays r->user
}

task<> validate(tracked_ptr<Request> r) {
    if (auto u = current_user.get(); !u || !(*u)->may(r)) throw forbidden();
    co_return;
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A request id and a user, set once by the handler and read by every
// function and task under it: the logger takes no arguments for them.
using namespace std::chrono_literals;

task_local<int> request_id;
task_local<string> user;

void log(const char* what) {                         // a plain function: the values of the task that called it
    std::cout << "[request " << request_id.get_or(0) << ", " << user.get_or("nobody") << "] " << what << "\n";
}

task<> store(int value) {
    log("storing");
    co_await sgcl::sleep(1ms);                       // the values survive a wait
    std::cout << "  value " << value << "\n";
    log("stored");
}

task<> audit() {
    co_await user.set("auditor");                    // this task's own: the handler keeps its user
    log("audited");
}

task<> handle(int id, string who, int value) {
    co_await request_id.set(id);
    co_await user.set(who);
    log("handling");
    co_await store(value);                           // an awaited task inherits both
    co_await spawn(audit());                   // a spawned one too, and sets one of its own
    co_await user.with("guest", store(value + 1));   // store runs as guest
    log("done");
}

int main() {
    spawn(handle(1, "ann", 10)).join();
    spawn(handle(2, "bob", 20)).join();
    log("outside a task");
    scheduler::stop();
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

- [executor](executor.md): where a task runs, remembered in the same header of the frame; [stop_token](stop_token.md): a token is one thing to keep in a task-local
- [coroutine](coroutine.md): `task`, `managed_frame`, the frame's header; [scheduler](scheduler.md): `spawn`, `go`
- [README: Coroutines](README.md#coroutines)
- `tests/async/task_local.cpp`: every behaviour above, checked.
