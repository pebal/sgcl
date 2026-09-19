# Select, AsyncSelect, Otherwise

```cpp
#include "sgcl/Sgcl/Async/Channel.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class... Cases> size_t Select(Cases... cases);          // blocks the thread; the index of the case served
    template<class... Cases> auto AsyncSelect(Cases... cases);       // co_await gives the index
    template<class F> auto Otherwise(F f);                           // the case taken when no other can be served at once

    // the cases, members of Channel<T>
    template<class F> auto Channel<T>::OnReceive(F f);               // f(T), or f(Optional<T>)
    template<class F> auto Channel<T>::OnSend(T value, F f = [] {});
    template<class F> auto Channel<void>::OnReceive(F f);            // f()
    template<class F> auto Channel<void>::OnSend(F f = [] {});
}
```

The same in the `sgcl` interface: [select, async_select, otherwise](../../async/select.md).

`Select` is the select of Go: a wait on several channels at once, each case a receive or a send with a body, the first case that can be served run and its index returned. It is what a loop that waits for "data or the signal to stop" is written with: `Select(jobs.OnReceive(handle), stop.OnReceive(quit))` parks the thread until either channel has something, and runs the body of the one that did. A thread calls `Select(...)` and blocks; a task writes `co_await AsyncSelect(...)` and holds no thread while it waits, the body running on the worker that resumes it. With an `Otherwise` case the select never waits: when no channel can be served at once, `Otherwise` is the case served (Go's `default`), which is how a poll is written. Among several cases ready at once one is chosen at random, so that a busy channel does not starve the others.

Nothing in it locks, as nothing in the channel does ([Channel](Channel.md)). A select that has to wait registers one waiter on each channel, and the waiters share one state, a managed object: the channel that serves a case wins it with a compare-exchange on that state, the other channels find their waiters claimed when they reach them and drop them, and the collector reclaims them. A select that registered and then sees a case it could serve cancels the state the same way and looks again. A coroutine's select publishes the state to the channels only after that look, so that no channel serves a case, resumes the coroutine and lets its owner destroy the channel while the look still reads it. The waiter of a send case carries the element, so a sender waiting in a select is served like any waiting sender, in its order.

## Rules

- A case is served, and its body run, when its channel does what the case asks: an element received (`OnReceive`), the element delivered (`OnSend`). A closed channel serves either case at once: a receive case with what was sent before the close, then with nothing; a send case with nothing delivered. The body of a receive case that takes `T` is called only with an element; one that takes `Optional<T>` is also called with `None`, for the close; the body of `Channel<void>::OnReceive` is called for a signal and for the close alike (both end a wait for the signal). The body of a send case is called when the element was delivered, not when the channel was closed on it. The index tells which case in every event.
- The bodies run after the wait, on the thread that called `Select`, or on the worker that resumed the task; never inside a channel operation. A body may do anything, including another select; a task's body is a plain function, and `co_await` belongs in the task after the select, not in the body.
- `Select` with no case that can ever be served and no `Otherwise` waits forever, as a receive on a channel nobody sends to does.
- The cases are taken by value: `OnSend(v)` copies or moves `v` into the case; a case object is for one select.
- One `Otherwise` at most; a select of one case is that case's operation with a body. A select must not have a send case and a receive case on one channel (each would find the other's waiter and wait for itself; Go's blocks): an assertion in debug builds.
- A coroutine that `co_await`s a select needs a managed frame, as one that awaits a channel does ([Task](Coroutine.md)).

## Members

### Select, AsyncSelect

```cpp
template<class... Cases> size_t Select(Cases... cases);
template<class... Cases> auto AsyncSelect(Cases... cases);   // an awaitable: co_await gives size_t
```

The index of the case served, counting from 0 in the order written, `Otherwise` included.

### The cases

```cpp
template<class F> auto Channel<T>::OnReceive(F f);               // f(T), or f(Optional<T>): also called with None on the close
template<class F> auto Channel<T>::OnSend(const T& value, F f = [] {});
template<class F> auto Channel<T>::OnSend(T&& value, F f = [] {});
template<class F> auto Channel<void>::OnReceive(F f);            // f(): a signal, or the close
template<class F> auto Channel<void>::OnSend(F f = [] {});
template<class F> auto Otherwise(F f);                           // f(): nothing could be served at once
```

```cpp
Channel<int> jobs(8);
Channel<int> results(8);
Channel<void> stop;
bool running = true;
while (running) {
    Select(
        jobs.OnReceive([&](int job) { results.Send(job * 2); }),      // served: an element came
        stop.OnReceive([&] { running = false; }),                     // served: a signal, or stop closed
        Otherwise([] { ThisThread::Yield(); })                  // nothing at once: a poll, not a wait
    );
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A worker that serves two queues and a stop signal: jobs come on one
// channel, urgent ones on another; the worker takes whichever has
// something, urgent or not, and ends on the signal. Everything it holds
// is managed; the channels hold the jobs while they wait.
struct Job {
    String name;
    int cost;
};

Task<int> Worker(Channel<Ptr<Job>>& jobs, Channel<Ptr<Job>>& urgent, Channel<void>& stop) {
    int done = 0;
    bool running = true;
    while (running) {
        co_await AsyncSelect(
            urgent.OnReceive([&](Ptr<Job> job) { done += job->cost; }),
            jobs.OnReceive([&](Ptr<Job> job) { done += job->cost; }),
            stop.OnReceive([&] { running = false; })
        );
    }
    co_return done;
}

int main() {
    Channel<Ptr<Job>> jobs(4), urgent(4);
    Channel<void> stop;
    Task w = Spawn(Worker(jobs, urgent, stop));
    for (int i : Range(10)) {
        jobs.Send(Make<Job>("job " + ToString(i), 1));
        if (i % 3 == 0) {
            urgent.Send(Make<Job>("urgent " + ToString(i), 10));
        }
    }
    while (!jobs.IsEmpty() || !urgent.IsEmpty()) {
        ThisThread::Yield();                            // the worker drains both
    }
    stop.Close();                                             // the worker's loop ends
    std::cout << "cost " << w.Join() << "\n";                 // cost 50
    return w.Result() == 50 ? 0 : 1;
}
```

The output:

```
cost 50
```

## See also

- [Channel](Channel.md): the channels a select waits on, and what a close does; [Task](Coroutine.md), [Scheduler](Scheduler.md): the tasks that `co_await` a select and where they run
- [README: Coroutines](../../async/README.md#coroutines)
- `tests/Sgcl/sgcl.cpp`: the behaviour above, checked.
