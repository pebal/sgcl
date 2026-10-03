[sgcl](../README.md) › [async](README.md)

# sgcl::async::once

```cpp
#include "sgcl/async/once.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class once;
}
```

`sgcl::async::once` runs a function once: the first caller of [call](once/call.md) runs it, and the others wait for
it to finish; Go's `sync.Once`, `std::call_once` with a wait that holds no thread. `call(f)` is an
[operation](operation.md), waited for as every wait of the module: `co_await o.call(f)` in a task, where the tasks
that lose wait holding no worker, and `o.call(f).wait()` on a thread. `co_await o.call(t)` runs the task `t` once,
and so does a coroutine function with captures given uncalled.

Under it is a channel of signals closed when the call is done, one of the family the [mutex](mutex.md) heads; a
once's waits are two of the forms a [channel](channel.md) has, the blocking and the awaitable, and a once is no case
of a [select](select.md).

What differs from Go's `sync.Once`: a function that throws has been called all the same, as a
[promise](promise.md) set with an exception is set, and its exception is not dropped: the first caller gets it
thrown, and so does every other caller, then and later. Go's `Do` returns quietly to every caller after a panic.

## Rules

- A once is an object, not a handle: it lives where a `tracked_ptr` may, on a stack or inside a managed object
  ([The rules](../core/README.md#the-rules), 1), and is neither copied nor moved. Tasks reach it through the
  object that holds it.
- A function that throws ends the once: the exception is kept, [called](once/called.md) is `true`, and the function
  is not run again; every caller, waiting or coming later, gets the exception thrown.
- `o.call(f).wait()` is a thread's: a task on a worker writes `co_await o.call(f)`
  ([README: The rules](README.md#the-rules), 1); debug builds assert.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](once/once.md) | constructs a once not yet called |
| `(destructor)` | destroys the once |

#### Operations

| Function | Description |
|---|---|
| [call](once/call.md) | runs the function by the first caller, the others waiting for it |

#### Observers

| Function | Description |
|---|---|
| [called](once/called.md) | checks whether the call is done |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Config {
    async::once loaded;
    atomic<int> loads = 0;
    string name;
};

async::task<string> name_of(tracked_ptr<Config> c) {
    co_await c->loaded.call([c] {  // the first caller loads, the others wait
        ++c->loads;
        c->name = "prod";
    });
    co_return c->name;
}

int main() {
    tracked_ptr c = make_tracked<Config>();
    vector<async::task<string>> readers;
    for (int i : range(4)) {
        readers.push_back(async::spawn(name_of(c)));
    }
    for (auto& r : readers) {
        println("{}", r.wait());
    }
    println("loaded {} time(s)", c->loads.load());
}
```

Output:

```text
prod
prod
prod
prod
loaded 1 time(s)
```

## See also

- [promise](promise.md): a value set once
- [event](event.md): a moment that happens once
- [mutex](mutex.md), [semaphore](semaphore.md), [wait_group](wait_group.md): the rest of the family
- [channel](channel.md): what it is made of
