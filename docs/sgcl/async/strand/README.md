[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::strand

```cpp
#include "sgcl/async/executor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class strand;
}
```

`sgcl::async::strand` is an [executor](../executor/README.md) with no thread of its own, Boost.Asio's strand: its tasks run on
the workers of the [scheduler](../scheduler/README.md), never two at once, in the order they were queued. It is serial access
to something without a lock: the handler of a connection, the owner of a document, a log three tasks append to. A
task moves to a strand with `co_await async::on(s)` ([on](../on.md)) and [spawn](spawn.md) starts one there; the
frame remembers the strand as it would an executor, so every wake of the task comes back through the strand.

The strand is a queue whose head is handed to the workers when the strand goes from idle to busy, and whose next
frame is handed over when the running task suspends or finishes; between the two nothing of the strand runs
anywhere. A task on a strand that waits leaves the strand to the next task, and comes back through the strand's
queue when it is woken, behind whoever is queued by then. The strand is held between two suspensions of a task and
never across one, so a task that reads a structure, awaits, and writes it does not find it as it left it: a
[mutex](../mutex/README.md) is what holds across a wait.

## Rules

- The tasks of a strand run one at a time, in the order their frames were queued, each on whichever worker the
  scheduler gives it; what one task wrote before it suspended is seen by the next, with no lock of the program's.
- A task on a strand that never suspends holds the strand, and its worker, until it does. Cooperative, as every task
  is.
- A strand destroyed leaves its tasks as an [executor](../executor/README.md) leaves them: the queue is a managed object the
  frames on it hold, and the tasks queued on it are never run again.
- The strand holds its queue through a root: it lives anywhere (a local, a global, a member). Neither copyable nor
  movable.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](strand.md) | constructs a strand with an empty queue |
| `(destructor)` | leaves the tasks still queued suspended for good |

#### Observers

| Function | Description |
|---|---|
| [busy](busy.md) | checks whether a task of the strand runs or is queued |

#### Starting tasks

| Function | Description |
|---|---|
| [spawn](spawn.md) | starts a task on the strand |
| [go](go.md) | starts a task on the strand and lets go of it |

## Complexity

A strand costs its queue and one count, and nothing while nothing is queued. A push is an exchange of the tail and
a store of a link, with no allocation; the push that finds the strand idle hands the frame to the workers, as an
enqueue does.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Document {
    vector<string> lines;  // touched by the strand's tasks only: no lock
};

async::task<> append(tracked_ptr<Document> doc, string who) {
    for (int i : range(3)) {
        doc->lines.push_back(who + to_string(i));
        co_await async::yield();  // the strand to the next task
    }
}

int main() {
    async::strand owner;
    tracked_ptr doc = make_tracked<Document>();
    auto a = owner.spawn(append(doc, "a"));
    auto b = owner.spawn(append(doc, "b"));
    a.wait();
    b.wait();
    println("{}", doc->lines);
}
```

Output:

```text
["a0", "b0", "a1", "b1", "a2", "b2"]
```

## See also

- [executor](../executor/README.md): the same queue, run by a thread of the program's choosing
- [on](../on.md): a task moved to a strand
- [mutex](../mutex/README.md): exclusion that holds across a wait
- [README: Coroutines](../README.md#coroutines)
