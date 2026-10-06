[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::singleflight

```cpp
#include "sgcl/async/singleflight.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class singleflight {
    public:
        friend bool operator==(const singleflight& a, const singleflight& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::singleflight` runs a function once per key at a time: Go's `golang.org/x/sync/singleflight.Group`.
The first caller of a key runs the function; every caller of the key that comes before the call ends waits for that
call and gets a copy of its result, or what it threw, rethrown; the first caller after the end runs the function
again. It is the cure of the cache stampede: a thousand requests for one cold entry make one fetch, and the other
nine hundred and ninety-nine wait for it, in tasks that hold no thread while they wait.

What differs from Go: the class is no template and the result is typed. [run](run.md) takes the result's type from
the function, a value or the result of the task of a coroutine function, and the caller gets that type, not an
`any`. The table of the calls in flight is sixteen shards, a hash map under a short lock each, held for a lookup and
never across `f`; a call that nobody joins makes no promise, and the first caller that joins makes it. There is no
third result telling whether the value was shared, and no `DoChan`: a task that awaits the call is what a select or
a timeout races.

## Rules

- A singleflight is a handle: one word, a tracked word to the table, which copies share;
  [operator==](operator_cmp.md) says whether two are the same table. A task takes it by value
  ([README: Handles](../README.md#handles)).
- The callers of one key must want one result type. A caller whose function gives another type than the call in
  flight for its key runs its function on its own, shared with nobody; a debug build asserts, as it is almost
  surely a mistake.
- The result is copied to every caller, so its type is copyable; a `string`, a `tracked_ptr` or a handle is one word
  to copy.
- A call runs to its end however many of its callers are gone: cancellation is the function's own, through a
  [stop_token](../stop_token/README.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](singleflight.md) | constructs an empty table, or a handle of the same table |
| `(destructor)` | lets go of the handle; the table is the collector's once no handle holds it |

#### Calls

| Function | Description |
|---|---|
| [run](run.md) | runs a function once for every caller of a key while it runs, in a task or on a thread |
| [forget](forget.md) | lets go of a key: the next caller runs the function anew |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same table |

## Complexity

A run is a lookup in one shard of the table and, for the first caller, an insertion, the call and an erasure; the
other callers wait on the call's promise.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

atomic<int> fetches = 0;

async::task<string> fetch(string key) {  // a slow lookup
    ++fetches;
    co_await async::sleep(10ms);
    co_return "value of " + key;
}

async::task<string> handler(async::singleflight flights, string key) {
    co_return co_await flights.run(key, [key] { return fetch(key); });
}

int main() {
    async::manual_clock clock;  // the time of the example: every request in before the fetch ends
    clock.install();
    async::singleflight flights;
    vector<async::task<string>> requests;
    for (int i : range(100)) {
        requests.push_back(async::spawn(handler(flights, "user:42")));
    }
    clock.advance(10ms);
    for (auto& r : requests) {
        r.wait();
    }
    println("{}", requests[0].wait());
    println("{} requests, {} fetches", requests.size(), fetches.load());
}
```

Output:

```text
value of user:42
100 requests, 1 fetches
```

## See also

- [once](../once/README.md): a function run once for good
- [promise](../promise/README.md): a completion awaited by many
- [map](../../core/map/README.md): the shards of the table
