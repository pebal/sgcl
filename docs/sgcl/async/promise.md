[sgcl](../README.md) › [async](README.md)

# sgcl::async::promise\<T\>

```cpp
#include "sgcl/async/promise.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T = void>
    class promise;

    template<>
    class promise<void>;
}
```

`sgcl::async::promise<T>` is a one-shot completion: a value, or an exception, that one side sets once and any number
of others wait for. It is the adapter between the callback APIs of a platform and `co_await`: an I/O completion
port, a dispatch queue, JNI, a driver's completion routine, a C library that takes a callback and a `void*` context,
all report on a thread of their own, and a task cannot wait for a callback. It waits for a promise: the callback calls
[set_value](promise/set_value.md) (or [set_exception](promise/set_exception.md)) and returns, and the task that wrote
`co_await p` is made ready with the value, having held no thread meanwhile. A thread waits with
[wait](promise/wait.md) and blocks; a [select](select.md) takes [on_done](promise/on_done.md) as a case, so that the
completion is bounded by a [timeout](timeout.md) or cancelled by a [stop token](stop_token.md) like any other wait.
What Java has as `CompletableFuture`, Kotlin as `CompletableDeferred`, Rust as a `oneshot` channel.

Under it an [event](event.md) with a value: a channel of signals closed by the set, so that the three forms of the
wait are the channel's, lock-free, the waiters reclaimed by the collector; the value lives in the promise's state
next to the channel. The shape is one state behind a handle, not `std::promise` and `std::future` apart: a completion
has one home, and whoever has a copy of the handle may set it or wait for it, any number of times for the waiting.
Exactly one setter wins, by a compare-exchange: a second `set_value` or `set_exception` is an error, asserted in debug
builds and ignored in release builds, where the first value stands.

## Rules

- A promise is a handle: one word, a tracked word to the state, which copies share; `==` says whether two are the
  same promise. It is made by the constructor, not set; there is no empty promise. It lives where a `tracked_ptr`
  may: on a stack, in a task (a parameter by value), in a managed object; in a global, a `std` container or the
  context a C library hands back, as a `rooted<async::promise<T>>` ([rooted](../core/rooted.md)), the same promise
  reached with `->` (the example below). Never a raw pointer kept in unmanaged memory alone, which keeps nothing
  alive; and a root is never part of a cycle: never a `rooted` in a managed object or a task's frame
  ([README: Handles](README.md#handles)).
- The value is set once; `set_value` and `set_exception` may be called from any thread, at any time, before or after
  the waits begin. A wait after the set does not wait. A value whose move into the promise throws sets the promise
  all the same, with that exception.
- `co_await p`, [wait](promise/wait.md) and [result](promise/result.md) give a reference to the value in the
  promise's state (`T&`), so every waiter reads the one value; a lone reader may move it out. Each rethrows what
  `set_exception` set, every time it is asked.
- A `tracked_ptr` value keeps its object for as long as the promise lives, as any member of a managed object does:
  the promise is the value's home until whoever awaited it has copied it out.
- `wait()` blocks the calling thread until the set, and so does `result()` on a promise not set yet: neither is called
  from a task on a worker, which writes `co_await p` (a debug build asserts).
- A promise nobody sets is a wait that never ends: the setter's side owns the obligation, as with a channel nobody
  closes. A select with a `timeout` case bounds the wait.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the value: an object type that is move-constructible; `set_value` of a `const T&` copies it. `void`, the default, is a completion without a value, a [specialization](#specializations). |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `awaiter` | the awaiter of `co_await p` ([wait, operator co_await](promise/wait.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](promise/promise.md) | makes a promise not set, or another handle of the same promise |
| `(destructor)` | lets go of the handle; the state is the collector's once no handle holds it |
| [operator=](promise/operator_assign.md) | makes the handle one of another promise |

#### Setting the result

| Function | Description |
|---|---|
| [set_value](promise/set_value.md) | sets the value and wakes the waiters |
| [set_exception](promise/set_exception.md) | sets an exception and wakes the waiters |

#### Getting the result

| Function | Description |
|---|---|
| [wait, operator co_await](promise/wait.md) | waits for the set and gives the value, on a thread or in a task |
| [result](promise/result.md) | the value, waited for first when the promise is not set |

#### Select cases

| Function | Description |
|---|---|
| [on_done](promise/on_done.md) | a case of a select served once the promise is set |

#### Observers

| Function | Description |
|---|---|
| [done](promise/done.md) | checks whether the promise is set |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](promise/operator_cmp.md) | checks whether two handles are of the same promise |

## Specializations

`promise<void>`, the default `promise<>`, is a completion without a value: [set_value](promise/set_value.md) takes
no argument, and [wait](promise/wait.md), `co_await` and [result](promise/result.md) give nothing, only rethrowing
what [set_exception](promise/set_exception.md) set. The rest is the primary's.

## Complexity

A set: a compare-exchange, the value moved in, and the close of the channel, which wakes every waiter. A wait on a
set promise: an atomic load; a wait that waits allocates its waiter, one managed object. The state is one managed
object with the channel's lists in it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A C library that works on a thread of its own and reports through a
// callback with a void* context
using completion = void (*)(void* context, int result);

void c_compute_async(int input, completion done, void* context) {
    thread([=] { done(context, input * 2); }).detach();
}

struct Context {
    rooted<async::promise<int>> done;  // unmanaged memory: the promise under a root
};

void on_computed(void* context, int result) {
    auto ctx = static_cast<Context*>(context);
    ctx->done->set_value(result);  // from the library's thread: the waiting task is made ready
    delete ctx;
}

async::task<int> compute(int input) {
    async::promise<int> done;  // the frame holds it, the context a copy under a root
    c_compute_async(input, on_computed, new Context{done});
    co_return co_await done;  // suspended until on_computed, no thread held
}

int main() {
    println("{}", async::spawn(compute(21)).wait());
}
```

Output:

```text
42
```

## See also

- [event](event.md): a completion without a value that any number wait for, set by any side
- [spawn_blocking](spawn_blocking.md): a blocking call off the workers, its result back the same way
- [channel](channel.md): what the wait is made of
- [select](select.md), [timeout](timeout.md): a wait bounded in time
- [rooted](../core/rooted.md): the promise held from unmanaged memory
- [README: Handles](README.md#handles)
