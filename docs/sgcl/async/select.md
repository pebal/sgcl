# sgcl::async::select, sgcl::async::select, sgcl::async::otherwise

```cpp
#include "sgcl/async/select.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class... Cases> size_t async::select(Cases... cases).wait();          // blocks the thread; the index of the case served
    template<class... Cases> auto async::select(Cases... cases);      // co_await gives the index
    template<class F> auto otherwise(F f);                           // the case taken when no other can be served at once

    // the cases, members of channel<T> (channel.h)
    template<class F> auto async::channel<T>::on_receive(F f);              // f(T), or f(optional<T>)
    template<class F> auto async::channel<T>::on_send(T value, F f = [] {});
    template<class F> auto async::channel<void>::on_receive(F f);           // f()
    template<class F> auto async::channel<void>::on_send(F f = [] {});
}
```

`select` is the select of Go: a wait on several channels at once, each case a receive or a send with a body, the first case that can be served run and its index returned. It is what a loop that waits for "data or the signal to stop" is written with: `async::select(jobs.on_receive(handle), stop.on_receive(quit))` parks the thread until either channel has something, and runs the body of the one that did. A thread calls `async::select(...)` and blocks; a coroutine with a managed frame writes `co_await async::select(...)` and holds no thread while it waits, the body running on the worker that resumes it. With an `otherwise` case the select never waits: when no channel can be served at once, `otherwise` is the case served (Go's `default`), which is how a poll is written. Among several cases ready at once one is chosen at random, so that a busy channel does not starve the others.

Nothing in it locks, as nothing in the channel does ([channel](channel.md)). A select that has to wait registers one waiter on each channel, and the waiters share one state, a managed object: the channel that serves a case wins it with a compare-exchange on that state, the other channels find their waiters claimed when they reach them and drop them, and the collector reclaims them. A select that registered and then sees a case it could serve cancels the state the same way and looks again. A coroutine's select publishes the state to the channels only after that look, so that no channel serves a case, resumes the coroutine and lets its owner destroy the channel while the look still reads it. The waiter of a send case carries the element, so a sender waiting in a select is served like any waiting sender, in its order.

## Rules

- A case is served, and its body run, when its channel does what the case asks: an element received (`on_receive`), the element delivered (`on_send`). A closed channel serves either case at once: a receive case with what was sent before the close, then with nothing; a send case with nothing delivered. The body of a receive case that takes `T` is called only with an element; one that takes `optional<T>` is also called with nothing, for the close; the body of `async::channel<void>::on_receive` is called for a signal and for the close alike (both end a wait for the signal). The body of a send case is called when the element was delivered, not when the channel was closed on it. The index tells which case in every event.
- The bodies run after the wait, on the thread that called `select`, or on the worker that resumed the coroutine; never inside a channel operation. A body may do anything, including another select; a coroutine's body is a plain function, and `co_await` belongs in the coroutine after the select, not in the body.
- `select` with no case that can ever be served and no `otherwise` waits forever, as a receive on a channel nobody sends to does.
- The cases are taken by value: `on_send(v)` copies or moves `v` into the case; a case object is for one select.
- One `otherwise` at most; a select of one case is that case's operation with a body. A select must not have a send case and a receive case on one channel (each would find the other's waiter and wait for itself; Go's blocks): an assertion in debug builds.
- A coroutine that `co_await`s a select needs a managed frame, as one that awaits a channel does ([coroutine](coroutine.md)).

## Members

### select, select

```cpp
template<class... Cases> size_t async::select(Cases... cases).wait();
template<class... Cases> auto async::select(Cases... cases);   // an awaitable: co_await gives size_t
```

The index of the case served, counting from 0 in the order written, `otherwise` included.

### The cases

```cpp
template<class F> auto async::channel<T>::on_receive(F f);              // f(T), or f(optional<T>): also called with nothing on the close
template<class F> auto async::channel<T>::on_send(const T& value, F f = [] {});
template<class F> auto async::channel<T>::on_send(T&& value, F f = [] {});
template<class F> auto async::channel<void>::on_receive(F f);           // f(): a signal, or the close
template<class F> auto async::channel<void>::on_send(F f = [] {});
template<class F> auto otherwise(F f);                           // f(): nothing could be served at once
```

```cpp
async::channel<int> jobs(8);
async::channel<int> results(8);
async::channel<void> stop;
bool running = true;
while (running) {
    async::select(
        jobs.on_receive([&](int job) { results.send(job * 2).wait(); }),     // served: an element came
        stop.on_receive([&] { running = false; }),                    // served: a signal, or stop closed
        otherwise([] { this_thread::yield(); })            // nothing at once: a poll, not a wait
    );
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A worker that serves two queues and a stop signal: jobs come on one
// channel, urgent ones on another; the worker takes whichever has
// something, urgent or not, and ends on the signal. Everything it holds
// is managed; the channels hold the jobs while they wait.
struct Job {
    string name;
    int cost;
};

async::task<int> worker(async::channel<tracked_ptr<Job>>& jobs, async::channel<tracked_ptr<Job>>& urgent, async::channel<void>& stop) {
    int done = 0;
    bool running = true;
    while (running) {
        co_await async::select(
            urgent.on_receive([&](tracked_ptr<Job> job) { done += job->cost; }),
            jobs.on_receive([&](tracked_ptr<Job> job) { done += job->cost; }),
            stop.on_receive([&] { running = false; })
        );
    }
    co_return done;
}

int main() {
    async::channel<tracked_ptr<Job>> jobs(4), urgent(4);
    async::channel<void> stop;
    auto w = async::spawn(worker(jobs, urgent, stop));
    for (int i : range(10)) {
        jobs.send(make_tracked<Job>("job " + to_string(i), 1)).wait();
        if (i % 3 == 0) {
            urgent.send(make_tracked<Job>("urgent " + to_string(i), 10)).wait();
        }
    }
    while (!jobs.empty() || !urgent.empty()) {
        this_thread::yield();                            // the worker drains both
    }
    stop.close();                                             // the worker's loop ends
    std::cout << "cost " << w.wait() << "\n";                 // cost 50
    return w.result() == 50 ? 0 : 1;
}
```

The output:

```
cost 50
```

## See also

- [channel](channel.md): the channels a select waits on, and what a close does; [coroutine](coroutine.md), [scheduler](scheduler.md): the coroutines that `co_await` a select and where they run
- [README: Coroutines](README.md#coroutines)
- `tests/async/select.cpp`: every behaviour above, checked.
