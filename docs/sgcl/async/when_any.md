[sgcl](../README.md) › [async](README.md)

# sgcl::async::when_any

```cpp
#include "sgcl/async/when.h"   // or "sgcl/async.h"

namespace sgcl::async {
    /*(1)*/ template<class... T>
            task<size_t> when_any(task<T>... ts);
    /*(2)*/ template<class T>
            task<size_t> when_any(vector<task<T>> ts);
}
```

Races the tasks given and gives the index of the first to finish: `co_await async::when_any(a, b)` in a task,
`async::when_any(a, b).wait()` on a thread. The others are let go of: they run on to their ends, and their frames
are the collector's then. A race whose losers are to stop gives the tasks a [stop_token](stop_token.md) and requests
the stop after the wait.

1. The index of the first of `ts` to finish; at least one task (a `static_assert` says so).
2. The same over a [vector](../core/vector.md) of tasks of one type; a vector of none gives `SIZE_MAX` at once, as
   nothing can finish first.

The tasks are taken over (moved in) and started by the call, each in a small task of its own that awaits it and
reports its end into a channel; the task returned receives once, the first report. So the tasks race whether they were
given spawned or not, and they run from the call on, before the returned task is awaited.

## Parameters

| Parameter | Description |
|---|---|
| `ts` | the tasks to race, taken over |

## Return value

A task, not started, whose `co_await` or `wait()` gives the index in `ts` of the first task to finish, or `SIZE_MAX`
for an empty vector (2).

## Complexity

Linear in the number of tasks: a task of its own per task given (a frame and a start), a channel with room for every
report, and one receive.

## Exceptions

- The call: `std::system_error` when a start of the tasks starts the scheduler and a worker's thread cannot be
  started.
- Carried out: what the first task to finish threw, rethrown, as `when_all` rethrows a task's exception.

A loser's exception is dropped with the loser, as its value is: never [on_unhandled](on_unhandled.md)'s.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> slow(async::channel<int> in) {
    auto v = co_await in.receive();
    co_return *v;
}

async::task<int> fast() {
    co_return 2;
}

int main() {
    async::channel<int> in;
    size_t first = async::when_any(slow(in), fast()).wait();
    println("{}", first);
    println("{}", in.send(5).wait());  // the loser runs on: it still waits, and takes the value

    vector<async::task<int>> none;
    println("{}", async::when_any(std::move(none)).wait() == SIZE_MAX);
}
```

Output:

```text
1
true
true
```

Losers stopped through a token:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> until_stopped(async::stop_token stop, async::channel<string> log) {
    co_await stop.stopped();
    co_await log.send("the loser stopped");
    co_return 0;
}

async::task<int> answer() {
    co_return 42;
}

int main() {
    async::stop_source src;
    async::channel<string> log(1);
    size_t first = async::when_any(until_stopped(src.token(), log), answer()).wait();
    src.request_stop();
    println("{}", first);
    println("{}", *log.receive().wait());
}
```

Output:

```text
1
the loser stopped
```

## See also

- [when_all](when_all.md): every result
- [with_timeout](with_timeout.md), [with_deadline](with_deadline.md): a task raced against a timer
- [select](select.md): a race of channels rather than tasks
- [stop_source](stop_source.md), [stop_token](stop_token.md): stopping the losers
- [task](task.md): what is raced
