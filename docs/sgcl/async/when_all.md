[sgcl](../README.md) › [async](README.md)

# sgcl::async::when_all

```cpp
#include "sgcl/async/when.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class... T>
    task<tuple<T...>> when_all(task<T>... ts);       // (1)
    template<class... Void>
        requires (std::is_void_v<Void> && ...)
    task<> when_all(task<Void>... ts);               // (2)
    template<class T>
    task<vector<T>> when_all(vector<task<T>> ts);    // (3)
    task<> when_all(vector<task<>> ts);              // (4)
}
```

Waits for every task given and gives their results: `co_await async::when_all(a, b, c)` in a task,
`async::when_all(a, b, c).wait()` on a thread. The results come in the order the tasks were given, not the order
they finished.

1. The results of tasks of values, none of them `task<void>`, as a [tuple](../core/aliases.md):
   `auto [a, b] = co_await when_all(...)`.
2. Tasks of nothing, all of them `task<void>`: nothing.
3. A [vector](../core/vector.md) of tasks of one type: a vector of their results.
4. A vector of `task<void>`: nothing.

`when_all` is a task itself, a coroutine with a managed frame that holds the tasks given (moved in: a task is awaited
by one awaiter, and the results of `when_all` are where theirs are). Like every task it is lazy, and it is awaited by
one coroutine, waited for by a thread, or spawned. It awaits the tasks one after another, in the order given, and the
`co_await` of a task nobody started starts it: tasks given spawned run at once, while tasks given unstarted,
`when_all(f(), g())`, run one after the other, `g` starting when `f` has ended. Tasks meant to run together are
given spawned, `when_all(async::spawn(f()), async::spawn(g()))`.

## Parameters

| Parameter | Description |
|---|---|
| `ts` | the tasks to wait for, taken over |

## Return value

A task, not started, whose `co_await` or `wait()` gives:

- (1) A `tuple<T...>` of the results, in the order of `ts`.
- (2), (4) Nothing.
- (3) A `vector<T>` of the results, in the order of `ts`.

## Complexity

Linear in the number of tasks: one frame for `when_all`, and an awaiter over each task's own, which installs the
`when_all` as the task's awaiter. (3) adds a vector of the results.

## Exceptions

- The call: none.
- Carried out: the exception of the first task, in the order of `ts`, that threw, rethrown once every task is done;
  `std::system_error` when the start of `when_all` or of a task nobody started starts the scheduler and a worker's
  thread cannot be started. That error ends `when_all` at once: the task whose start failed runs, queued, when the
  workers next start, and the tasks after it are let go of unstarted.

`when_all` waits for all, whatever any threw, so nothing of it runs on unobserved. The exceptions of the other tasks
are dropped, never [on_unhandled](on_unhandled.md)'s.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>

using namespace sgcl;

async::task<string> fetch(string what, int ms) {
    co_await async::sleep(std::chrono::milliseconds(ms));
    co_return what + "!";
}

// Three fetches at once, assembled in the order given
async::task<string> page() {
    auto [head, body, foot] = co_await async::when_all(async::spawn(fetch("head", 20)),
                                                       async::spawn(fetch("body", 30)),
                                                       async::spawn(fetch("foot", 10)));
    co_return head + body + foot;
}

int main() {
    println("{}", page().wait());

    vector<async::task<string>> parts;
    for (int i : range(3)) {
        parts.push_back(async::spawn(fetch(to_string(i), 30 - i * 10)));
    }
    println("{}", async::when_all(std::move(parts)).wait());
}
```

Output:

```text
head!body!foot!
["0!", "1!", "2!"]
```

Tasks given unstarted run one after the other:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> step(int i) {
    println("start {}", i);
    co_await async::yield();
    println("end {}", i);
    co_return i;
}

int main() {
    auto [a, b] = async::when_all(step(1), step(2)).wait();
    println("{} {}", a, b);
}
```

Output:

```text
start 1
end 1
start 2
end 2
1 2
```

The first exception in the order given, once every task has ended:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

async::task<> check(const char* name, int x) {
    co_await async::yield();
    if (x < 0) {
        throw std::invalid_argument(name);
    }
}

int main() {
    try {
        async::when_all(async::spawn(check("first", 1)),
                        async::spawn(check("second", -2)),
                        async::spawn(check("third", -3))).wait();
    } catch (const std::exception& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
second
```

## See also

- [when_any](when_any.md): the first task to finish
- [task_group](task_group.md): a scope of tasks stopped as one by the first exception
- [select](select.md): a race of channels rather than tasks
- [spawn](spawn.md): starts the tasks to wait for
- [task](task.md): what is composed
