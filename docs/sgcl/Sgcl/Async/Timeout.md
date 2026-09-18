# Timeout, WithDeadline, TimedOut

```cpp
#include "sgcl/Sgcl/Async/Timeout.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T> Task<Optional<T>> Timeout(Task<T> t, Duration d);                     // the result, or None when d passed first (Task<bool> for a Task<>)
    template<class T> Task<Optional<T>> Timeout(Task<T> t, Duration d, StopSource loser);   // the same, the source stopped when d passed first
    template<class T> Task<T> WithDeadline(Task<T> t, Duration d);                          // the result, or TimedOut thrown
    template<class T> Task<T> WithDeadline(Task<T> t, Duration d, StopSource loser);
    template<class T> Task<T> WithDeadline(Task<T> t, StopToken deadline);                  // the token's stop as the deadline
    using TimedOut = sgcl::timed_out;                                                        // : std::runtime_error, "timed out"
}
```

The same in the `sgcl` interface: [timeout, with_deadline, timed_out](../../async/timeout.md).

A timeout on a task: what the [`Timeout(d, f)` case](Time.md) is for a Select, and Go's `select { case r := <-done: case <-time.After(d): }` is. `co_await Timeout(t, d)` is the task's result as an `Optional`, `None` when `d` passed first (`true` or `false` for a task of nothing); `co_await WithDeadline(t, d)` is the result itself, or the exception `TimedOut` when `d` passed first; `WithDeadline(t, token)` the same with a token's stop as the deadline, for a source given a `StopAfter` or stopped by hand (Go's `context.WithTimeout` handed down). Each is a task, waited for as one: awaited, joined from a thread (`Timeout(t, d).Join()`), spawned or not (a wait starts it).

A race is what it is: the task is awaited by a small task that finishes into a slot (a managed object with the result, the exception and a [Channel](Channel.md) closed when they are there), and the timeout task selects between that channel and the timer's ([After](Time.md), or the token's), so the two waits are one [Select](Select.md) and neither holds a thread. A task that lost the race is not stopped by the timeout on its own, since a task cannot be stopped from outside: it runs on to its end, its result lands in the slot nobody reads any more, and the slot and the frames are the collector's. To have the loser stop, give it a token whose source the timeout may stop: `Timeout(t, d, source)` requests the stop of the source when `d` passes first, so a task made with `source.Token()` sees it and leaves. A result that came at the same instant as the deadline is a result: the slot is looked at whichever case the select served, and a task done before the race has its result taken without one.

## Rules

- The task given is consumed: it is started if nobody started it, awaited by the race, and its result comes back through the timeout or not at all.
- What the task threw comes through the timeout as it is, before any deadline: `Timeout` and `WithDeadline` rethrow it.
- The loser runs on unless it was made with the token of the `StopSource` given as the third argument, which the timeout stops when the deadline passes first; the source is left alone when the task finished in time. The source is a child of the caller's token where the caller has one (`StopSource src(tok)`), so the caller's stop reaches the task too.
- `WithDeadline(t, token)` stops nothing: a task made with the same token stops itself. An empty token (`StopToken()`) is no deadline.
- A timeout of zero is a deadline that has passed: a task done already gives its result, any other is `None` or `TimedOut`, and runs on. (A deadline of zero is still a timer: a task that finishes in the few microseconds before the timer thread fires it wins the race.)
- `TimedOut` is a `std::runtime_error` whose `what()` is `"timed out"`.

## Members

```cpp
template<class T> Task<Optional<T>> Timeout(Task<T> t, Duration d);                      // Task<bool> for a Task<>
template<class T> Task<Optional<T>> Timeout(Task<T> t, Duration d, StopSource loser);
template<class T> Task<T> WithDeadline(Task<T> t, Duration d);
template<class T> Task<T> WithDeadline(Task<T> t, Duration d, StopSource loser);
template<class T> Task<T> WithDeadline(Task<T> t, StopToken deadline);

using TimedOut = sgcl::timed_out;
```

```cpp
Task<int> Fetch(StopToken tok) {
    size_t which = co_await AsyncSelect(
        Timeout(300ms, [] {}),                                        // the work: here, a wait
        tok.OnStop([] {})                                             // or the stop
    );
    co_return which == 0 ? 42 : -1;
}

Task<> Caller(StopToken tok) {
    StopSource src(tok);                                              // the task's own source, under the caller's token
    Optional<int> r = co_await Timeout(Fetch(src.Token()), 50ms, src);   // None after 50 ms, and Fetch stopped
    StopSource own;
    try {
        int v = co_await WithDeadline(Fetch(own.Token()), 1s);        // 42, in time
    } catch (const TimedOut&) {
    }
    StopSource deadline;
    deadline.StopAfter(1s);
    int w = co_await WithDeadline(Fetch(deadline.Token()), deadline.Token());   // the same deadline for both
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A lookup with a budget: the fast source answers in time, the slow one
// does not and is stopped through its source; a third is given a
// deadline and throws. Nothing here holds a thread while it waits.
Task<String> Lookup(String name, int ms, StopToken tok) {
    size_t which = co_await AsyncSelect(
        Timeout(std::chrono::milliseconds(ms), [] {}),             // the work
        tok.OnStop([] {})                                          // or the stop
    );
    if (which == 1) {
        std::cout << name << " stopped\n";
    }
    co_return name + " found";
}

int main() {
    StopSource fast, slow, late;
    auto a = Timeout(Lookup("fast", 10, fast.Token()), 100ms, fast).Join();
    std::cout << (a ? *a : String("fast timed out")) << "\n";
    auto b = Timeout(Lookup("slow", 500, slow.Token()), 50ms, slow).Join();
    ThisThread::SleepFor(20ms);                                    // the slow one sees its stop
    std::cout << (b ? *b : String("slow timed out")) << "\n";
    try {
        WithDeadline(Lookup("late", 500, late.Token()), 50ms).Join();   // not stopped: runs on
    } catch (const TimedOut& e) {
        std::cout << "late: " << e.what() << "\n";
    }
    ThisThread::SleepFor(500ms);                                   // the late one finishes on its own, unseen
    return a && !b && slow.IsStopRequested() ? 0 : 1;
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

- [Sleep, After, Tick, Timeout](Time.md): the `Timeout` case of a Select, `After`, `Sleep`; [StopToken](StopToken.md): `StopAfter`, the token given to the loser; [WhenAll, WhenAny](When.md): `WhenAny`, a race of tasks; [TaskGroup](TaskGroup.md): a scope of tasks stopped as one
- `tests/Sgcl/task_group_and_timeout.cpp`: the behaviour above, checked.
