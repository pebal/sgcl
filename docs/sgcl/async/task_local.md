[sgcl](../README.md) › [async](README.md)

# sgcl::async::task_local\<T\>

```cpp
#include "sgcl/async/task_local.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    class task_local {
    public:
        class setter;
    };
}
```

`sgcl::async::task_local<T>` is a value visible to a task and to the tasks it starts, read without passing it
through every signature: a request id, a deadline, a [stop_token](stop_token.md), a logger, the current user. Go's
`context.WithValue`, Kotlin's `CoroutineContext`, tokio's `task_local!`. A `task_local` is declared once, at namespace
scope, and the variable is the key: `async::task_local<int> request_id;`. `co_await request_id.set(7)` sets the value
for the task from that line on, and for every task it starts from then on; `request_id.get()` reads it, from the
coroutine's body or from any function it calls, however deep; `request_id.with(7, t)` is a task that runs `t` with
the value set.

The values live in managed nodes, one per set, chained newest first, and the head of a task's chain is a word at the
front of its frame, next to the [executor](executor.md) the frame remembers; the variable itself holds nothing and
lives anywhere. A task started by another ([spawn](spawn.md), [go](go.md), a `co_await` of a task nobody started, an
executor's or a strand's `spawn`) takes the head of the starting task's chain as it is at that moment: inheritance is
one word copied. A node is never changed once made, so a child that sets a value puts a node of its own in front of
the shared tail, and the parent and the siblings keep what they had: copy on write by construction. A task that sets
the same key twice reads the newest.

`get()` works from a plain function because the thread knows which frame it runs: the scheduler, an executor and a
task resumed by hand set a thread-local to the frame around every resume. The same thread-local is what a start
reads to inherit from, so a task started from a plain thread, outside any task, inherits nothing.

## Rules

- A `task_local<T>` is a variable at namespace scope (or a static), one per key; it is neither copyable nor movable.
  Its values are in managed nodes: `T` may hold a `tracked_ptr`, and lives as a member of a managed object does.
- [set](task_local/set.md) is `co_await`ed: the `co_await` is what names the coroutine whose value it is. It never
  suspends. A function the task calls reads; the task sets.
- What a task sets after starting a child is the task's own; what a child sets is the child's. Nothing a task sets is
  seen by its parent or its siblings.
- A value is copied out by [get](task_local/get.md): a `tracked_ptr`, a string, a token, an int, whatever is cheap
  to copy and safe to share between the tasks that inherit it. The nodes are never written after they are made, so
  the tasks that share one read it freely, from any thread.
- `get()` outside a task (a plain thread, a worker between two tasks) is `nullopt`, and
  [get_or](task_local/get_or.md) the fallback.
- The chain is read from the front: a task that sets many keys, or sets one key many times, walks them all for a
  miss. A handful of keys is what this is for.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the value: moved into a node by `set`, copied out by `get` and `get_or`. |

## Member types

| Type | Definition |
|---|---|
| `setter` | the awaitable [set](task_local/set.md) returns: `co_await` it to set the value |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](task_local/task_local.md) | constructs the key |
| `(destructor)` | does nothing: the values are in the tasks' nodes |

#### Observers

| Function | Description |
|---|---|
| [get](task_local/get.md) | the value of the running task, or nothing |
| [get_or](task_local/get_or.md) | the value of the running task, or a fallback |
| [is_set](task_local/is_set.md) | checks whether the running task has a value |

#### Modifiers

| Function | Description |
|---|---|
| [set](task_local/set.md) | sets the value for the task and the tasks it starts from then on |
| [with](task_local/with.md) | a task that runs another with the value set |

## Complexity

- A set: a node made and linked, constant.
- A read: linear in the number of sets in the task's chain, newest first.
- An inheritance: one word copied at the child's start.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

// A request id and a user, set once by the handler and read by every function
// and task under it: the logger takes no arguments for them.
async::task_local<int> request_id;
async::task_local<string> user;

void log(const char* what) {  // a plain function: the values of the task that called it
    println("[request {}, {}] {}", request_id.get_or(0), user.get_or("nobody"), what);
}

async::task<> store(int value) {
    log("storing");
    co_await async::sleep(1ms);  // the values survive a wait
    println("  value {}", value);
    log("stored");
}

async::task<> audit() {
    co_await user.set("auditor");  // this task's own: the handler keeps its user
    log("audited");
}

async::task<> handle(int id, string who, int value) {
    co_await request_id.set(id);
    co_await user.set(who);
    log("handling");
    co_await store(value);  // an awaited task inherits both
    co_await async::spawn(audit());  // a spawned one too, and sets one of its own
    co_await user.with("guest", store(value + 1));
    log("done");
}

int main() {
    async::spawn(handle(1, "ann", 10)).wait();
    async::spawn(handle(2, "bob", 20)).wait();
    log("outside a task");
}
```

Output:

```text
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

- [executor](executor.md): where a task runs, remembered in the same header of the frame
- [stop_token](stop_token.md): a token is one thing to keep in a task-local
- [task](task.md), [spawn](spawn.md): the tasks that inherit the values
- [coroutine](../core/coroutine.md): the managed frame under a task
- [README: Coroutines](README.md#coroutines)
