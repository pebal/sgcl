[sgcl](../../README.md) › [async](../README.md) › [singleflight](README.md)

# sgcl::async::singleflight::run

```cpp
template<class F>
auto run(const string& key, F f) const;
```

Runs `f` once for every caller of `key` while it runs, and gives each a copy of its result: Go's `Do`. The call does
nothing yet; it returns an object of its own type, nodiscard as an [operation](../operation/README.md) is and carried
out the same two ways ([README: Waiting operations](../README.md#waiting-operations)), a program keeping it in `auto`
at most:

- `co_await flights.run(key, f)` in a task: a caller that finds a call of the key in flight suspends until it
  ends, holding no thread.
- `flights.run(key, f).wait()` on a thread: the same, blocking the thread.

The first caller of the key runs `f` itself, where it runs, as Go's does: a function is called, and the task of a
coroutine function (one that returns a [task](../task/README.md)) is awaited, or waited for on a thread. The call
ends when `f` returns or throws; the key is free from then on, and the next caller runs `f` again. What `f` throws
is rethrown to the caller that ran it and to every caller that waited for the call. A function that returns an
`expected` passes it on as it is, the error included.

Takes part only when `f` can be called with no arguments.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the call; any string, the empty one too |
| `f` | the function, or the coroutine function, that gives the result |

## Return value

The object of the call. Carried out, it gives the result of the call: what `f` returns, or what its task gives
(`void` for a function of nothing).

## Complexity

A lookup in the table, and for the first caller an insertion, `f` and an erasure, each under the lock of one of the
table's sixteen shards, never held across `f`; the other callers wait, the first of them making the call's promise.

## Exceptions

The call: what the copy of `f` throws. Carried out: what `f` throws, for every caller of the call, and what the copy
of the result throws, for the caller copying it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    async::singleflight flights;
    int calls = 0;
    int a = flights.run("answer", [&] { ++calls; return 42; }).wait();
    int b = flights.run("answer", [&] { ++calls; return 43; }).wait();  // the first call has ended
    println("{} {} {}", a, b, calls);
    try {
        flights.run("broken", []() -> int { throw std::runtime_error("down"); }).wait();
    } catch (const std::exception& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
42 43 2
down
```

## See also

- [forget](forget.md): the key let go of before the call ends
- [sgcl::async::singleflight](README.md)
