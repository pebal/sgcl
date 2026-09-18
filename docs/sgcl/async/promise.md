# sgcl::promise

```cpp
#include "sgcl/async/promise.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T = void>
    class promise;   // a one-shot completion: set once by anyone, awaited by a task, blocked on by a thread, a case of a select
}
```

The same class in the `Sgcl` interface: [Promise](../Sgcl/Async/Promise.md).

The adapter between the callback APIs of a platform and `co_await`: an I/O completion port, a dispatch queue, JNI, a driver's completion routine, a C library that takes a callback and a `void*` context, all report on a thread of their own, and a task cannot wait for a callback. It waits for a promise: the callback calls `set_value(v)` (or `set_exception(e)`) and returns, and the task that wrote `co_await p` is made ready with the value, having held no thread meanwhile. A thread waits with `get()` and blocks; a select takes `p.on_ready(f)` as a case, so that the completion is bounded by a [timeout](timer.md) or cancelled by a [stop token](stop_token.md) like any other wait. What Java has as `CompletableFuture`, Kotlin as `CompletableDeferred`, Rust as a `oneshot` channel.

Under it an [event](event.md) with a value: a channel of signals closed by the set, so that the three forms of the wait are the channel's, lock-free, the waiters reclaimed by the collector; the value lives in the promise's object next to the channel. The shape is one object and no shared state, not `std::promise` and `std::future` apart: a completion has one home, and whoever has a pointer to it may set it or wait for it, any number of times for the waiting. Exactly one setter wins, by a compare-exchange: a second `set_value` or `set_exception` is an error, asserted in debug builds and ignored in release, where the first value stands.

## Rules

- A `promise` lives where a `tracked_ptr` may: on a stack, in a coroutine's frame, or inside a managed object, `make_tracked<promise<int>>()` included ([The rules](../core/README.md#the-rules), 1); it is neither copied nor moved. A callback from a foreign thread reaches it through a `root_ptr` in its context (the example below) or a `tracked_ptr` captured in a managed object; never through a raw pointer kept in unmanaged memory alone, which keeps nothing alive.
- The value is set once; `set_value` and `set_exception` from any thread, at any time, before or after the waits begin. A wait after the set does not wait. A value whose move into the promise throws sets the promise all the same, with that exception.
- `co_await p` and `get()` give a reference to the value in the promise (`T&`), so every waiter reads the one value; a lone reader may move it out. Either rethrows what `set_exception` set, every time it is asked.
- A `tracked_ptr` value keeps its object for as long as the promise lives, as any member of a managed object does: the promise is the value's home until whoever awaited it has copied it out.
- `get()` blocks the calling thread: not from a task on a worker, which `co_await`s (debug builds assert).
- A promise nobody sets is a wait that never ends: the setter's side owns the obligation, as with a channel nobody closes. A select with a `timeout` case bounds the wait.

## Members

```cpp
promise();

void set_value(const T& v);  void set_value(T&& v);   // promise<void>: set_value()
void set_exception(std::exception_ptr e);             // the waiters get the exception instead
bool ready() const noexcept;                          // set already

T& get();                                             // a thread: blocks until set; the value, or the exception rethrown
T& result();                                          // the value of a ready promise, or the exception rethrown
awaiter operator co_await() noexcept;                 // a task: co_await p, the same, no thread held
template<class F> auto on_ready(F f);                 // a case of a select: f() once ready (the value from p.result())
```

```cpp
sgcl::task<int> awaits(sgcl::promise<int>& p) {
    co_return co_await p;                             // the value, when it is set; no thread held
}
sgcl::task<int> awaits_briefly(sgcl::promise<int>& p) {
    int v = -1;
    co_await sgcl::async_select(                      // a task waits, but not forever
        p.on_ready([&] { v = p.result(); }),
        sgcl::timeout(1s, [] { /* gave up */ })
    );
    co_return v;
}
void setter_and_getter() {
    sgcl::promise<int> p;
    sgcl::thread th([&] { p.set_value(42); });        // any thread sets it, once
    int v = p.get();                                  // a thread waits instead: 42
    th.join();
    sgcl::promise<> done;                             // a completion without a value
    done.set_value();
    done.get();                                       // set already: no wait
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cstring>
#include <iostream>
#include <thread>

// A C library that does its work on a thread of its own and reports
// through a callback with a void* context: a promise turns that into a
// co_await. The context is unmanaged memory (the library hands back a
// void*), so it holds the promise through a root_ptr.
using completion = void (*)(void* context, int result);

void c_read_async(const char* text, char* buffer, completion done, void* context) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));   // the I/O
        std::strcpy(buffer, text);
        done(context, (int)std::strlen(text));
    }).detach();
}

struct Context {
    sgcl::root_ptr<sgcl::promise<int>> done;
};

void on_read(void* context, int result) {
    auto ctx = static_cast<Context*>(context);
    ctx->done->set_value(result);                             // from the library's thread: the task that awaits is made ready
    delete ctx;
}

sgcl::task<int> read_line(char* buffer) {
    sgcl::tracked_ptr done = sgcl::make_tracked<sgcl::promise<int>>();   // a managed object: the frame holds it, the context holds it too
    c_read_async("hello", buffer, on_read, new Context{done});
    co_return co_await *done;                                            // suspended until on_read, no thread held
}

int main() {
    char buffer[64];
    int n = sgcl::spawn(read_line(buffer)).join();
    std::cout << "read " << n << " bytes: " << buffer << "\n";
    sgcl::scheduler::stop();
    return n == 5 ? 0 : 1;
}
```

The output:

```
read 5 bytes: hello
```

## See also

- [blocking](blocking.md): `spawn_blocking`, a blocking call whose result comes back through a promise; [event](event.md): the promise without a value that any number wait for; [channel](channel.md): what the wait is made of; [select](select.md), [timer](timer.md): bounding it
- [root_ptr](../core/root_ptr.md): the promise held from unmanaged memory
- `tests/async/promise.cpp`: every behaviour above, checked.
