# sgcl::async::with_timeout, sgcl::async::with_deadline, sgcl::async::timed_out, sgcl::async::stopped

```cpp
#include "sgcl/async/timeout.h"   // or "sgcl/sgcl.h"

namespace sgcl::async {
    template<class T> task<expected<T, timed_out>> with_timeout(task<T> t, duration d);                        // the result, or timed_out when d passed first
    template<class T> task<expected<T, timed_out>> with_timeout(task<T> t, duration d, stop_source loser);     // the same, the source stopped when d passed first
    template<class T> task<expected<T, timed_out>> with_deadline(task<T> t, time_point when);                  // the same by a point of the clock
    template<class T> task<expected<T, timed_out>> with_deadline(task<T> t, time_point when, stop_source loser);
    template<class T> task<expected<T, stopped>> with_deadline(task<T> t, stop_token token);                   // the token's stop as the deadline
    class timed_out;                                                                                          // message(): "timed out"
    class stopped;                                                                                            // message(): "stopped"
}
```

A timeout on a task: what the [`async::timeout(d, f)` case](timer.md#timeout) is for a select, and Go's `select { case r := <-done: case <-time.After(d): }` is. `co_await async::with_timeout(t, d)` is the task's result, or the error `timed_out` when `d` passed first, and `async::with_deadline(t, when)` the same by a point of the module's clock (Go's `context.WithTimeout` and `WithDeadline`); `async::with_deadline(t, token)` the same with a token's stop as the deadline, for a source given a `stop_after`, a `stop_at` or stopped by hand (Go's context handed down), whose error is `stopped`: the library cannot tell a deadline on the source from a stop by hand, as Go tells `DeadlineExceeded` from `Canceled`. A deadline that passed is a failure the library reports, so it is an [`expected`](../core/expected.md), as every failure of the library is; a task of nothing gives `expected<void, …>`. Each is a task, waited for as one: awaited, waited for from a thread (`async::with_timeout(t, d).wait()`), spawned or not (a wait starts it).

A race against a duration is one managed object and no frame of its own: the raced task is held by it and given a continuation that is a call into it instead of a coroutine to resume, the deadline is a [timer](timer.md) that calls into it, and the first call wins with a compare-exchange on the race's word and hands the timeout task's frame to the scheduler; the second finds the word taken and does nothing. A task that won cancels the timer, which is swept out of the heap with the closed ones; a deadline that won leaves the task to run on, its end calling into a race that is over. A race against a token is a [select](select.md) between the task's finish, reported into a slot by a small runner task, and the token's channel, since a token's stop is a channel closed and there is nothing to arm or cancel. A task that lost the race is not stopped by the timeout on its own, since a task cannot be stopped from outside: it runs on to its end, its result lands where nobody reads it any more, and the objects and the frames are the collector's. To have the loser stop, give it a token whose source the timeout may stop: `async::with_timeout(t, d, source)` requests the stop of the source when `d` passes first, so a task made with `source.token()` sees it. A result that came at the same instant as the deadline is a result: the task is looked at whichever call won. Measured ([benchmarks](benchmarks.md)): about 500 ns per race of a task that returns at once, against 1870 for the runner task, the race task and the select it was before.

## Rules

- The task given is consumed: it is started if nobody started it, awaited by the race, and its result comes back through the timeout or not at all.
- What the task threw comes through the timeout as it is, before any deadline: an exception of the task's is the task's, not a failure of the timeout.
- The loser runs on unless it was made with the token of the `stop_source` given as the third argument, which the timeout stops when the deadline passes first; the source is left alone when the task finished in time. The source is a child of the caller's token where the caller has one (`async::stop_source src(tok)`), so the caller's stop reaches the task too.
- `async::with_deadline(t, token)` stops nothing: a task made with the same token stops itself. An empty token (`async::stop_token()`) is no deadline.
- A timeout of zero, or a point that has passed, is a deadline that has passed: a task done already gives its result, any other is `timed_out`, and runs on. (A deadline of zero is still a timer: a task that finishes in the few microseconds before the timer thread fires it wins the race.)
- `timed_out` and `stopped` are values, not exceptions; each answers `message()` and compares equal to another of its kind.

## Members

```cpp
template<class T> async::task<expected<T, async::timed_out>> async::with_timeout(async::task<T> t, duration d);
template<class T> async::task<expected<T, async::timed_out>> async::with_timeout(async::task<T> t, duration d, async::stop_source loser);
template<class T> async::task<expected<T, async::timed_out>> async::with_deadline(async::task<T> t, time_point when);
template<class T> async::task<expected<T, async::timed_out>> async::with_deadline(async::task<T> t, time_point when, async::stop_source loser);
template<class T> async::task<expected<T, async::stopped>> async::with_deadline(async::task<T> t, async::stop_token token);

class timed_out { public: string message() const; };   // "timed out"
class stopped { public: string message() const; };     // "stopped"
```

```cpp
async::task<int> fetch(async::stop_token tok) {
    size_t which = co_await async::select(
        async::timeout(300ms, [] {}),                                    // the work: here, a wait
        tok.on_stop([] {})                                              // or the stop
    );
    co_return which == 0 ? 42 : -1;
}

async::task<> caller(async::stop_token tok) {
    async::stop_source src(tok);                                         // the task's own source, under the caller's token
    auto r = co_await async::with_timeout(fetch(src.token()), 50ms, src);   // timed_out after 50 ms, and fetch stopped
    async::stop_source own;
    auto v = co_await async::with_timeout(fetch(own.token()), 1s);      // 42, in time
    async::stop_source deadline;
    deadline.stop_after(1s);
    auto w = co_await async::with_deadline(fetch(deadline.token()), deadline.token());   // the same deadline for both
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

using namespace std::chrono_literals;

// A lookup with a budget: the fast source answers in time, the slow one
// does not and is stopped through its source; a third is given a
// deadline and runs on. Nothing here holds a thread while it waits.
async::task<string> lookup(string name, int ms, async::stop_token tok) {
    size_t which = co_await async::select(
        async::timeout(std::chrono::milliseconds(ms), [] {}),       // the work
        tok.on_stop([] {})                                         // or the stop
    );
    if (which == 1) {
        std::cout << name << " stopped\n";
    }
    co_return name + " found";
}

int main() {
    async::stop_source fast, slow, late;
    auto a = async::with_timeout(lookup("fast", 10, fast.token()), 100ms, fast).wait();
    std::cout << (a ? *a : a.error().message()) << "\n";
    auto b = async::with_timeout(lookup("slow", 500, slow.token()), 50ms, slow).wait();
    this_thread::sleep_for(20ms);                            // the slow one sees its stop
    std::cout << (b ? *b : "slow " + b.error().message()) << "\n";
    auto c = async::with_timeout(lookup("late", 500, late.token()), 50ms).wait();   // not stopped: runs on
    std::cout << "late: " << (c ? *c : c.error().message()) << "\n";
    this_thread::sleep_for(500ms);                           // the late one finishes on its own, unseen
    return a && !b && !c && slow.stop_requested() ? 0 : 1;
}
```

The output:

```
fast found
slow stopped
slow timed out
late: timed out
```

## See also

- [timer](timer.md): the `timeout` case of a select, `after`, `sleep`; [stop_token](stop_token.md): `stop_after`, `stop_at`, the token given to the loser; [when](when.md): `when_any`, a race of tasks; [task_group](task_group.md): a scope of tasks stopped as one
- `tests/async/timeout.cpp`: every behaviour above, checked.
