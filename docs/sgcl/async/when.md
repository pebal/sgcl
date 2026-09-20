# sgcl::when_all, sgcl::when_any

```cpp
#include "sgcl/async/when.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class... T> task<std::tuple<T...>> when_all(task<T>... ts);   // every result, as a tuple
    template<class... Void> task<> when_all(task<Void>... ts);            // tasks of nothing
    template<class T> task<vector<T>> when_all(vector<task<T>> ts);       // a range of tasks: a vector of results
    task<> when_all(vector<task<>> ts);
    template<class... T> task<size_t> when_any(task<T>... ts);            // the index of the first to finish
    template<class T> task<size_t> when_any(vector<task<T>> ts);
}
```

The composition of tasks. `co_await when_all(a, b, c)` waits for every task and gives their results as a tuple (a `vector` for a range of tasks of one type, nothing for tasks of nothing), in the order the tasks were given, not the order they finished; what any of them threw is rethrown. `co_await when_any(a, b, c)` gives the index of the first task to finish and lets go of the rest: they run on to their ends, and their frames are the collector's then. A thread writes the same with `join()`: `when_all(a, b).join()`.

Both are tasks themselves, coroutines with a managed frame that hold the tasks given: a `when_all` is a frame with the tasks in it and one `co_await` at a time, a `when_any` spawns a small task per task given that finishes into a channel, and receives once. The tasks are taken over (moved in): a task is awaited by one awaiter, and the result of `when_all` is where theirs are. They are spawned ones, or ones nobody started: a task starts with the first wait for it ([coroutine](coroutine.md#join)), so `when_all(f(), g())` starts both.

## Rules

- The tasks given are consumed: the `task` objects are moved into the composition, and their results come back through it (`when_all`) or not at all (`when_any`).
- `when_all` waits for all, whatever any threw: the exception is rethrown once every task is done, so nothing runs on unobserved.
- `when_any` lets the others run on: for a race whose losers should stop, give the tasks a [stop_token](stop_token.md) and request the stop after the `co_await`. `when_any` gives what the first to finish threw, as `when_all` gives a task's exception; a loser's exception is dropped with the loser. `when_any` of an empty vector gives `SIZE_MAX` at once: nothing can finish first.
- A `when_all` or a `when_any` is a task: awaited by one coroutine, joined by a thread, spawned or not (a wait starts it).

## Members

```cpp
template<class... T> task<std::tuple<T...>> when_all(task<T>... ts);
template<class... Void> requires (std::is_void_v<Void> && ...) task<> when_all(task<Void>... ts);
template<class T> task<vector<T>> when_all(vector<task<T>> ts);
task<> when_all(vector<task<>> ts);
template<class... T> task<size_t> when_any(task<T>... ts);
template<class T> task<size_t> when_any(vector<task<T>> ts);
```

```cpp
task<int> number() { co_return 1; }
task<string> text() { co_return "text"; }
task<int> fetch(int i) { co_await sgcl::sleep(std::chrono::milliseconds(i)); co_return i; }

auto [n, s] = when_all(spawn(number()), spawn(text())).join();   // int, string
vector<task<int>> tasks;
for (int i : range(8)) {
    tasks.push_back(fetch(i));                                 // not spawned: when_all starts them
}
vector<int> results = when_all(std::move(tasks)).join();
size_t first = when_any(fetch(1), fetch(2)).join();     // 0 or 1: whichever finished first
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

using namespace std::chrono_literals;

// A page assembled from three fetches at once, and a lookup raced between
// two sources with the slower one abandoned. Every task is a frame on the
// managed heap; nothing here holds a thread while it waits.
task<string> fetch(string what, int ms) {
    co_await sgcl::sleep(std::chrono::milliseconds(ms));
    co_return what + "!";
}

task<string> page() {
    auto [head, body, foot] = co_await when_all(fetch("head", 20), fetch("body", 30), fetch("foot", 10));
    co_return head + body + foot;
}

int main() {
    std::cout << page().join() << "\n";                                            // head!body!foot!
    size_t winner = when_any(fetch("slow", 50), fetch("fast", 5)).join();
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

- [coroutine](coroutine.md): `task`, what is composed; [select](select.md): a race of channels rather than tasks; [stop_token](stop_token.md): stopping the losers
- `tests/async/when.cpp`: every behaviour above, checked.
