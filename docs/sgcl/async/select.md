[sgcl](../README.md) › [async](README.md)

# sgcl::async::select

```cpp
#include "sgcl/async/select.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class... Cases>
    [[nodiscard]] /* unspecified */ select(Cases... cases)
        noexcept((std::is_nothrow_move_constructible_v<Cases> && ...));
}
```

The select of Go: a wait on several channels at once, each case a receive or a send with a body, the first case that
can be served run and its index given. It is what a loop that waits for "data or the signal to stop" is written with: a
receive case on the channel of the data and one on the channel of the stop, and the select waits until either has
something and runs the body of the one that had. A task writes `co_await async::select(...)` and holds no thread while it waits, the
body running on the worker that resumes it; a thread writes `async::select(...).wait()` and blocks
([README: Waiting operations](README.md#waiting-operations)). With an [otherwise](otherwise.md) case the select
never waits: when no other case can be served at once, `otherwise` is the case served (Go's `default`), which is how
a poll is written. Among several cases ready at once one is chosen at random, so that a busy channel does not starve
the others.

The cases are what the module's waits give for a select, each a channel's case under its name:
[channel::on_receive](channel/on_receive.md) and [channel::on_send](channel/on_send.md), a broadcast
subscription's [on_receive](broadcast-subscription/on_receive.md), a promise's [on_done](promise/on_done.md), an
event's [on_set](event/on_set.md), a mutex's [on_lock](mutex/on_lock.md), a semaphore's
[on_acquire](semaphore/on_acquire.md), the `on_done` of a [wait_group](wait_group/on_done.md), a
[task_group](task_group/on_done.md) and a [blocking_task](blocking_task/on_done.md), a stop token's
[on_stop](stop_token/on_stop.md), [timeout](timeout.md), and [otherwise](otherwise.md).

Nothing in it locks, as nothing in the channel does. A select that has to wait registers one waiter on each channel,
and the waiters share one state, a managed object: the channel that serves a case wins it with a compare-exchange on
that state, the other channels find their waiters claimed when they reach them and drop them, and the collector
reclaims them. A select that registered and then sees a case it could serve cancels the state the same way and looks
again. A task's select publishes the state to the channels only after that look, so that no channel serves a case,
resumes the task and lets its owner destroy the channel while the look still reads it. The waiter of a send case
carries the element, so a sender waiting in a select is served like any waiting sender, in its order.

## Parameters

| Parameter | Description |
|---|---|
| `cases` | the cases, taken by value: at least one, and at most one `otherwise` (a `static_assert` says so otherwise) |

## Return value

An awaitable of its own, of an unspecified type (kept in `auto`, if at all) and marked nodiscard, which does nothing
until it is carried out: `co_await` in a task, or its `wait()` on a thread. Either gives a `size_t`: the index of the
case served, counting from 0 in the order the cases were written, `otherwise` included. The body of that case has run
by then.

## Complexity

Linear in the number of cases: each is tried once, from a random one on. A select that waits allocates its state and
a waiter per case, managed objects; a case of a broadcast subscription may allocate its round.

## Exceptions

- The call: what the move constructors of the cases throw (their bodies, a send case's element); none when they are
  noexcept.
- Carried out: what the body of the case served throws; `std::system_error` when a case served wakes a waiting task
  and the wake starts the scheduler's workers, one of which cannot be started; what the move constructor and the move
  assignment of a send case's element throw.

A move of a case's element that throws on the side that serves the case (a send case's element into the buffer,
an element into a receive case) comes out of the select, the channel otherwise as it was.

## Notes

- A case is served, and its body run, when its channel does what the case asks: an element received, an element
  delivered, a promise set, a lock taken. A closed channel serves its cases at once: a receive case with what was sent
  before the close, then with nothing; a send case with nothing delivered, without its body. The index tells which
  case was served in every event.
- The bodies run after the wait, on the thread that called the select or on the worker that resumed the task; never
  inside a channel operation. A body may do anything, another select included; in a task it is a plain function, and
  a `co_await` belongs in the task after the select, not in the body.
- A select with no case that can ever be served and no `otherwise` waits forever, as a receive on a channel nobody
  sends to does.
- The cases are taken by value: a case object is for one select. A select of one case is that case's operation with
  a body.
- A select must not have a send case and a receive case on one channel, which would each find the other's waiter and
  wait for itself (Go's blocks): a debug build asserts.
- A task that awaits a select needs a managed frame, as one that awaits a channel does
  ([README: Coroutines](README.md#coroutines)).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> worker(async::channel<int> jobs, async::channel<void> stop) {
    int done = 0;
    for (bool running = true; running;) {
        co_await async::select(
            jobs.on_receive([&](int cost) { done += cost; }),
            stop.on_receive([&] { running = false; }));  // a signal, or stop closed
    }
    co_return done;
}

int main() {
    async::channel<int> jobs;  // a rendezvous: each send waits for the worker
    async::channel<void> stop;
    async::task<int> w = async::spawn(worker(jobs, stop));
    for (int cost : {1, 2, 3, 4}) {
        jobs.send(cost).wait();
    }
    stop.close();
    println("cost {}", w.wait());

    async::channel<int> a(1), b(1);
    b.send(5).wait();
    size_t served = async::select(
        a.on_receive([](int v) { println("a: {}", v); }),
        b.on_receive([](int v) { println("b: {}", v); })).wait();  // a thread blocks
    println("case {}", served);
}
```

Output:

```text
cost 10
b: 5
case 1
```

## See also

- [otherwise](otherwise.md): the case taken when no other can be served at once
- [channel](channel/README.md): the channels a select waits on, and what a close does
- [timeout](timeout.md): a case served after a while
- [task](task/README.md), [scheduler](scheduler/README.md): the tasks that await a select and where they run
- [README: Waiting operations](README.md#waiting-operations)
