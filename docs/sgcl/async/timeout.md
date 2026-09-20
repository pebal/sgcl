# sgcl::timeout, sgcl::with_deadline, sgcl::timed_out

```cpp
#include "sgcl/async/timeout.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T> task<optional<T>> timeout(task<T> t, duration d);                     // the result, or nullopt when d passed first (task<bool> for a task<>)
    template<class T> task<optional<T>> timeout(task<T> t, duration d, stop_source loser);  // the same, the source stopped when d passed first
    template<class T> task<T> with_deadline(task<T> t, duration d);                         // the result, or timed_out thrown
    template<class T> task<T> with_deadline(task<T> t, duration d, stop_source loser);
    template<class T> task<T> with_deadline(task<T> t, stop_token deadline);                // the token's stop as the deadline
    class timed_out;                                                                         // : std::runtime_error, "timed out"
}
```

A timeout on a task: what the [`timeout(d, f)` case](timer.md#timeout) is for a select, and Go's `select { case r := <-done: case <-time.After(d): }` is. `co_await timeout(t, d)` is the task's result as an `optional`, `nullopt` when `d` passed first (`true` or `false` for a task of nothing); `co_await with_deadline(t, d)` is the result itself, or the exception `timed_out` when `d` passed first; `with_deadline(t, token)` the same with a token's stop as the deadline, for a source given a `stop_after` or stopped by hand (Go's `context.WithTimeout` handed down). Each is a task, waited for as one: awaited, joined from a thread (`timeout(t, d).join()`), spawned or not (a wait starts it).

A race against a duration is one managed object and no frame of its own: the raced task is held by it and given a continuation that is a call into it instead of a coroutine to resume, the deadline is a [timer](timer.md) that calls into it, and the first call wins with a compare-exchange on the race's word and hands the timeout task's frame to the scheduler; the second finds the word taken and does nothing. A task that won cancels the timer, which is swept out of the heap with the closed ones; a deadline that won leaves the task to run on, its end calling into a race that is over. A race against a token is a [select](select.md) between the task's finish, reported into a slot by a small runner task, and the token's channel, since a token's stop is a channel closed and there is nothing to arm or cancel. A task that lost the race is not stopped by the timeout on its own, since a task cannot be stopped from outside: it runs on to its end, its result lands where nobody reads it any more, and the objects and the frames are the collector's. To have the loser stop, give it a token whose source the timeout may stop: `timeout(t, d, source)` requests the stop of the source when `d` passes first, so a task made with `source.token()` sees it. A result that came at the same instant as the deadline is a result: the task is looked at whichever call won. Measured ([benchmarks](benchmarks.md)): 510 ns per race of a task that returns at once, against 1870 for the runner task, the race task and the select it was before.

## Rules

- The task given is consumed: it is started if nobody started it, awaited by the race, and its result comes back through the timeout or not at all.
- What the task threw comes through the timeout as it is, before any deadline: `timeout` and `with_deadline` rethrow it.
- The loser runs on unless it was made with the token of the `stop_source` given as the third argument, which the timeout stops when the deadline passes first; the source is left alone when the task finished in time. The source is a child of the caller's token where the caller has one (`stop_source src(tok)`), so the caller's stop reaches the task too.
- `with_deadline(t, token)` stops nothing: a task made with the same token stops itself. An empty token (`stop_token()`) is no deadline.
- A timeout of zero is a deadline that has passed: a task done already gives its result, any other is `nullopt` or `timed_out`, and runs on. (A deadline of zero is still a timer: a task that finishes in the few microseconds before the timer thread fires it wins the race.)
- `timed_out` is a `std::runtime_error` whose `what()` is `"timed out"`.

## Members

```cpp
template<class T> task<optional<T>> timeout(task<T> t, duration d);                      // task<bool> for a task<>
template<class T> task<optional<T>> timeout(task<T> t, duration d, stop_source loser);
template<class T> task<T> with_deadline(task<T> t, duration d);
template<class T> task<T> with_deadline(task<T> t, duration d, stop_source loser);
template<class T> task<T> with_deadline(task<T> t, stop_token deadline);

class timed_out : public std::runtime_error { public: timed_out(); };
```

```cpp
sgcl::task<int> fetch(sgcl::stop_token tok) {
    size_t which = co_await sgcl::async_select(
        sgcl::timeout(300ms, [] {}),                                    // the work: here, a wait
        tok.on_stop([] {})                                              // or the stop
    );
    co_return which == 0 ? 42 : -1;
}

sgcl::task<> caller(sgcl::stop_token tok) {
    sgcl::stop_source src(tok);                                         // the task's own source, under the caller's token
    sgcl::optional<int> r = co_await sgcl::timeout(fetch(src.token()), 50ms, src);   // nullopt after 50 ms, and fetch stopped
    sgcl::stop_source own;
    try {
        int v = co_await sgcl::with_deadline(fetch(own.token()), 1s);   // 42, in time
    } catch (const sgcl::timed_out&) {
    }
    sgcl::stop_source deadline;
    deadline.stop_after(1s);
    int w = co_await sgcl::with_deadline(fetch(deadline.token()), deadline.token());   // the same deadline for both
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A lookup with a budget: the fast source answers in time, the slow one
// does not and is stopped through its source; a third is given a
// deadline and throws. Nothing here holds a thread while it waits.
sgcl::task<sgcl::string> lookup(sgcl::string name, int ms, sgcl::stop_token tok) {
    size_t which = co_await sgcl::async_select(
        sgcl::timeout(std::chrono::milliseconds(ms), [] {}),       // the work
        tok.on_stop([] {})                                         // or the stop
    );
    if (which == 1) {
        std::cout << name << " stopped\n";
    }
    co_return name + " found";
}

int main() {
    sgcl::stop_source fast, slow, late;
    auto a = sgcl::timeout(lookup("fast", 10, fast.token()), 100ms, fast).join();
    std::cout << (a ? *a : sgcl::string("fast timed out")) << "\n";
    auto b = sgcl::timeout(lookup("slow", 500, slow.token()), 50ms, slow).join();
    sgcl::this_thread::sleep_for(20ms);                            // the slow one sees its stop
    std::cout << (b ? *b : sgcl::string("slow timed out")) << "\n";
    try {
        sgcl::with_deadline(lookup("late", 500, late.token()), 50ms).join();   // not stopped: runs on
    } catch (const sgcl::timed_out& e) {
        std::cout << "late: " << e.what() << "\n";
    }
    sgcl::this_thread::sleep_for(500ms);                           // the late one finishes on its own, unseen
    return a && !b && slow.stop_requested() ? 0 : 1;
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

- [timer](timer.md): the `timeout` case of a select, `after`, `sleep`; [stop_token](stop_token.md): `stop_after`, the token given to the loser; [when](when.md): `when_any`, a race of tasks; [task_group](task_group.md): a scope of tasks stopped as one
- `tests/async/timeout.cpp`: every behaviour above, checked.
