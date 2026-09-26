# sgcl::async::when_all, sgcl::async::when_any

```cpp
#include "sgcl/async/when.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class... T> async::task<std::tuple<T...>> async::when_all(async::task<T>... ts);   // every result, as a tuple
    template<class... Void> async::task<> async::when_all(async::task<Void>... ts);            // tasks of nothing
    template<class T> async::task<vector<T>> async::when_all(vector<async::task<T>> ts);       // a range of tasks: a vector of results
    async::task<> async::when_all(vector<async::task<>> ts);
    template<class... T> async::task<size_t> async::when_any(async::task<T>... ts);            // the index of the first to finish
    template<class T> async::task<size_t> async::when_any(vector<async::task<T>> ts);
}
```

The composition of tasks. `co_await async::when_all(a, b, c)` waits for every task and gives their results as a tuple (a `vector` for a range of tasks of one type, nothing for tasks of nothing), in the order the tasks were given, not the order they finished; what any of them threw is rethrown. `co_await async::when_any(a, b, c)` gives the index of the first task to finish and lets go of the rest: they run on to their ends, and their frames are the collector's then. A thread writes the same with `wait()`: `async::when_all(a, b).wait()`.

Both are tasks themselves, coroutines with a managed frame that hold the tasks given: a `when_all` is a frame with the tasks in it and one `co_await` at a time, a `when_any` spawns a small task per task given that finishes into a channel, and receives once. The tasks are taken over (moved in): a task is awaited by one awaiter, and the result of `when_all` is where theirs are. They are spawned ones, or ones nobody started: a task starts with the first wait for it ([coroutine](coroutine.md#join)), so `async::when_all(f(), g())` starts both.

## Rules

- The tasks given are consumed: the `task` objects are moved into the composition, and their results come back through it (`when_all`) or not at all (`when_any`).
- `when_all` waits for all, whatever any threw: the exception is rethrown once every task is done, so nothing runs on unobserved.
- `when_any` lets the others run on: for a race whose losers should stop, give the tasks a [stop_token](stop_token.md) and request the stop after the `co_await`. `when_any` gives what the first to finish threw, as `when_all` gives a task's exception; a loser's exception is dropped with the loser. `when_any` of an empty vector gives `SIZE_MAX` at once: nothing can finish first.
- A `when_all` or a `when_any` is a task: awaited by one coroutine, joined by a thread, spawned or not (a wait starts it).

## Members

```cpp
template<class... T> async::task<std::tuple<T...>> async::when_all(async::task<T>... ts);
template<class... Void> requires (std::is_void_v<Void> && ...) async::task<> async::when_all(async::task<Void>... ts);
template<class T> async::task<vector<T>> async::when_all(vector<async::task<T>> ts);
async::task<> async::when_all(vector<async::task<>> ts);
template<class... T> async::task<size_t> async::when_any(async::task<T>... ts);
template<class T> async::task<size_t> async::when_any(vector<async::task<T>> ts);
```

```cpp
async::task<int> number() { co_return 1; }
async::task<string> text() { co_return "text"; }
async::task<int> fetch(int i) { co_await async::sleep(std::chrono::milliseconds(i)); co_return i; }

auto [n, s] = async::when_all(async::spawn(number()), async::spawn(text())).wait();   // int, string
vector<async::task<int>> tasks;
for (int i : range(8)) {
    tasks.push_back(fetch(i));                                 // not spawned: when_all starts them
}
vector<int> results = async::when_all(std::move(tasks)).wait();
size_t first = async::when_any(fetch(1), fetch(2)).wait();     // 0 or 1: whichever finished first
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
async::task<string> fetch(string what, int ms) {
    co_await async::sleep(std::chrono::milliseconds(ms));
    co_return what + "!";
}

async::task<string> page() {
    auto [head, body, foot] = co_await async::when_all(fetch("head", 20), fetch("body", 30), fetch("foot", 10));
    co_return head + body + foot;
}

int main() {
    std::cout << page().wait() << "\n";                                            // head!body!foot!
    size_t winner = async::when_any(fetch("slow", 50), fetch("fast", 5)).wait();
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
