[sgcl](../README.md) › [async](README.md)

# sgcl::async::wait_group

```cpp
#include "sgcl/async/wait_group.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class wait_group {
    public:
        friend bool operator==(const wait_group& a, const wait_group& b) noexcept;
    };
}
```

`sgcl::async::wait_group` counts work down to zero: [add](wait_group/add.md) counts the work, [done](wait_group/done.md)
counts it off, and the wait waits for the count to reach zero; Go's `sync.WaitGroup`, Java's `CountDownLatch` that
may count up again. A group whose count came back from zero, an `add` after the work was done, is waited for again,
as Go's is. Under it is a channel per round, closed when the count reaches zero and replaced by the `add` that
starts the next round; the old ones are the collector's.

It is one of the family the [mutex](mutex.md) heads, each a channel of signals under the name of what it does, and
is waited for as a task is: `g.wait()` on a thread, `co_await g` in a task, which holds no thread while it waits,
and `g.on_done(f)` as a case of a [select](select.md). A [task_group](task_group.md) is a wait group over the tasks
it starts, with their exceptions and their cancellation.

## Rules

- A wait group is a handle: one word, a tracked word to the state, which copies share;
  [operator==](wait_group/operator_cmp.md) says whether two are the same group. It is made at zero by its
  constructor, and there is no empty group ([README: Handles](README.md#handles)). The tasks that count off take it
  by value.
- The `add` comes before the work starts and the wait after, in the program's order, as Go's do: an `add` from zero
  racing with a wait is a misuse. So is a `done` without its `add`, which nothing detects.
- `add(-n)` takes *n* off, as `done()` takes one: a count brought to zero releases the waiters, Go's `Add(-n)`.
- `wait()` is a thread's: a task on a worker writes `co_await g` ([README: The rules](README.md#the-rules), 1).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](wait_group/wait_group.md) | constructs a group at zero, or a handle of the same group |
| `(destructor)` | lets go of the handle; the state is the collector's once no handle holds it |
| [operator=](wait_group/operator_assign.md) | makes the handle one of another group |

#### Modifiers

| Function | Description |
|---|---|
| [add](wait_group/add.md) | counts work in, or off with a negative count |
| [done](wait_group/done.md) | counts one off |

#### Observers

| Function | Description |
|---|---|
| [count](wait_group/count.md) | the work not yet counted off |

#### Waiting

| Function | Description |
|---|---|
| [wait, operator co_await](wait_group/wait.md) | waits for the count to reach zero, on a thread or in a task |
| [on_done](wait_group/on_done.md) | a case of a select: a call once the count is zero |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](wait_group/operator_cmp.md) | checks whether two handles are the same group |

## Complexity

`add` and `done` are an atomic add on the count; the `add` that starts a round makes the round's channel, and the
`done` that ends it closes the channel, waking every waiter. A wait at zero is a look at the count.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

using namespace std::chrono_literals;

// A crawler with a bound: at most three fetches at once (the semaphore), a
// shared count under a mutex, a wait group that knows when all is done, and
// an event that starts them together. Every wait of a task holds no thread.
struct Site {
    async::semaphore slots{3};
    async::mutex lock;
    async::wait_group pending;
    async::event go;
    int fetched = 0;  // guarded by lock
};

async::task<> fetch(tracked_ptr<Site> site) {
    co_await site->go;  // all start together
    co_await site->slots.acquire();  // three at a time
    co_await async::sleep(1ms);  // the fetch
    {
        auto guard = co_await site->lock.scoped_lock();
        ++site->fetched;
    }
    site->slots.release();
    site->pending.done();
}

int main() {
    tracked_ptr site = make_tracked<Site>();
    for (int page : range(20)) {
        site->pending.add();
        async::go(fetch(site));
    }
    site->go.set();
    site->pending.wait();  // this thread waits for the twenty
    println("{} pages", site->fetched);
}
```

Output:

```text
20 pages
```

## See also

- [task_group](task_group.md): a wait group with the tasks' exceptions and cancellation
- [when_all](when_all.md): every result back
- [event](event.md): a moment that happens once
- [mutex](mutex.md), [semaphore](semaphore.md), [once](once.md): the rest of the family
- [channel](channel.md): what it is made of; [select](select.md): its case
- [README: Handles](README.md#handles)
