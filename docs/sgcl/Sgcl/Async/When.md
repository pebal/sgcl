# WhenAll, WhenAny

```cpp
#include "sgcl/Sgcl/Async/When.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class... T> Task<std::tuple<T...>> WhenAll(Task<T>... ts);   // every result, as a tuple
    template<class... Void> Task<> WhenAll(Task<Void>... ts);            // tasks of nothing
    template<class T> Task<List<T>> WhenAll(List<Task<T>> ts);           // a List of tasks: a List of results
    Task<> WhenAll(List<Task<>> ts);
    template<class... T> Task<size_t> WhenAny(Task<T>... ts);            // the index of the first to finish
    template<class T> Task<size_t> WhenAny(List<Task<T>> ts);
}
```

The same in the `sgcl` interface: [when_all, when_any](../../async/when.md).

The composition of tasks. `co_await WhenAll(a, b, c)` waits for every task and gives their results as a tuple (a `List` for a `List` of tasks of one type, nothing for tasks of nothing), in the order the tasks were given, not the order they finished; what any of them threw is rethrown. `co_await WhenAny(a, b, c)` gives the index of the first task to finish and lets go of the rest: they run on to their ends, and their frames are the collector's then. A thread writes the same with `Join()`: `WhenAll(a, b).Join()`.

Both are tasks themselves, coroutines with a managed frame that hold the tasks given: a `WhenAll` is a frame with the tasks in it and one `co_await` at a time, a `WhenAny` spawns a small task per task given that finishes into a channel, and receives once. The tasks are taken over (moved in): a task is awaited by one awaiter, and the result of `WhenAll` is where theirs are. They are spawned ones, or ones nobody started: a task starts with the first wait for it ([Task](Coroutine.md#join)), so `WhenAll(F(), G())` starts both.

## Rules

- The tasks given are consumed: the `Task` objects are moved into the composition, and their results come back through it (`WhenAll`) or not at all (`WhenAny`).
- `WhenAll` waits for all, whatever any threw: the exception is rethrown once every task is done, so nothing runs on unobserved.
- `WhenAny` lets the others run on: for a race whose losers should stop, give the tasks a [StopToken](StopToken.md) and request the stop after the `co_await`. `WhenAny` gives what the first to finish threw, as `WhenAll` gives a task's exception; a loser's exception is dropped with the loser. `WhenAny` of an empty list gives `SIZE_MAX` at once: nothing can finish first.
- A `WhenAll` or a `WhenAny` is a task: awaited by one coroutine, joined by a thread, spawned or not (a wait starts it).

## Members

```cpp
template<class... T> Task<std::tuple<T...>> WhenAll(Task<T>... ts);
template<class... Void> requires (std::is_void_v<Void> && ...) Task<> WhenAll(Task<Void>... ts);
template<class T> Task<List<T>> WhenAll(List<Task<T>> ts);
Task<> WhenAll(List<Task<>> ts);
template<class... T> Task<size_t> WhenAny(Task<T>... ts);
template<class T> Task<size_t> WhenAny(List<Task<T>> ts);
```

```cpp
Task<int> Number() { co_return 1; }
Task<String> Text() { co_return "text"; }
Task<int> Fetch(int i) { co_await Sleep(std::chrono::milliseconds(i)); co_return i; }

auto [n, s] = WhenAll(Spawn(Number()), Spawn(Text())).Join();   // int, String
List<Task<int>> tasks;
for (int i : Range(8)) {
    tasks.Add(Fetch(i));                                       // not spawned: WhenAll starts them
}
List<int> results = WhenAll(std::move(tasks)).Join();
size_t first = WhenAny(Fetch(1), Fetch(2)).Join();            // 0 or 1: whichever finished first
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A page assembled from three fetches at once, and a lookup raced between
// two sources with the slower one abandoned. Every task is a frame on the
// managed heap; nothing here holds a thread while it waits.
Task<String> Fetch(String what, int ms) {
    co_await Sleep(std::chrono::milliseconds(ms));
    co_return what + "!";
}

Task<String> Page() {
    auto [head, body, foot] = co_await WhenAll(Fetch("head", 20), Fetch("body", 30), Fetch("foot", 10));
    co_return head + body + foot;
}

int main() {
    std::cout << Page().Join() << "\n";                                            // head!body!foot!
    size_t winner = WhenAny(Fetch("slow", 50), Fetch("fast", 5)).Join();
    std::cout << (winner == 1 ? "the fast one" : "the slow one") << "\n";          // the fast one
    return winner == 1 ? 0 : 1;
}
```

The output:

```
head!body!foot!
the fast one
```

## See also

- [Task](Coroutine.md): what is composed; [Select](Select.md): a race of channels rather than tasks; [StopToken](StopToken.md): stopping the losers
- `tests/Sgcl/sgcl.cpp`: the behaviour above, checked.
