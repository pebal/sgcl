# Promise

```cpp
#include "sgcl/Sgcl/Async/Promise.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T = void>
    class Promise;   // a one-shot completion: set once by anyone, awaited by a task, blocked on by a thread, a case of a Select
}
```

The same class in the `sgcl` interface: [promise](../../async/promise.md).

The adapter between the callback APIs of a platform and `co_await`: an I/O completion port, a dispatch queue, JNI, a driver's completion routine, a C library that takes a callback and a `void*` context, all report on a thread of their own, and a task cannot wait for a callback. It waits for a `Promise`: the callback calls `SetValue(v)` (or `SetException(e)`) and returns, and the task that wrote `co_await p` is made ready with the value, having held no thread meanwhile. A thread waits with `Get()` and blocks; a [Select](Select.md) takes `p.OnReady(f)` as a case, so that the completion is bounded by a [Timeout](Time.md) or cancelled by a [StopToken](StopToken.md) like any other wait. What Java has as `CompletableFuture`, Kotlin as `CompletableDeferred`, Rust as a `oneshot` channel.

Under it an [Event](Event.md) with a value: a channel of signals closed by the set, so that the three forms of the wait are the channel's, lock-free, the waiters reclaimed by the collector; the value lives in the promise's object next to the channel. The shape is one object and no shared state, not `std::promise` and `std::future` apart: a completion has one home, and whoever has a pointer to it may set it or wait for it, any number of times for the waiting. Exactly one setter wins, by a compare-exchange: a second `SetValue` or `SetException` is an error, asserted in debug builds and ignored in release, where the first value stands.

## Rules

- A `Promise` lives where a `Ptr` may: on a stack, in a coroutine's frame, or inside a managed object, `Make<Promise<int>>()` included ([The rules](../../core/README.md#the-rules), 1); it is neither copied nor moved. A callback from a foreign thread reaches it through a `RootPtr` in its context (the example below) or a `Ptr` captured in a managed object; never through a raw pointer kept in unmanaged memory alone, which keeps nothing alive.
- The value is set once; `SetValue` and `SetException` from any thread, at any time, before or after the waits begin. A wait after the set does not wait. A value whose move into the promise throws sets the promise all the same, with that exception.
- `co_await p` and `Get()` give a reference to the value in the promise (`T&`), so every waiter reads the one value; a lone reader may move it out. Either rethrows what `SetException` set, every time it is asked.
- A `Ptr` value keeps its object for as long as the promise lives, as any member of a managed object does.
- `Get()` blocks the calling thread: not from a task on a worker, which `co_await`s (debug builds assert).
- A promise nobody sets is a wait that never ends: the setter's side owns the obligation, as with a channel nobody closes. A `Select` with a `Timeout` case bounds the wait.

## Members

```cpp
Promise();

void SetValue(const T& v);  void SetValue(T&& v);     // Promise<void>: SetValue()
void SetException(std::exception_ptr e);              // the waiters get the exception instead
bool IsReady() const noexcept;                        // set already

T& Get();                                             // a thread: blocks until set; the value, or the exception rethrown
T& Result();                                          // the value of a ready promise, or the exception rethrown
auto operator co_await() noexcept;                    // a task: co_await p, the same, no thread held
template<class F> auto OnReady(F f);                  // a case of a Select: f() once ready (the value from p.Result())
sgcl::promise<T>& Inner() noexcept;
```

```cpp
Task<int> Awaits(Promise<int>& p) {
    co_return co_await p;                             // the value, when it is set; no thread held
}
Task<int> AwaitsBriefly(Promise<int>& p) {
    int v = -1;
    co_await AsyncSelect(                             // a task waits, but not forever
        p.OnReady([&] { v = p.Result(); }),
        Timeout(1s, [] { /* gave up */ })
    );
    co_return v;
}
void SetterAndGetter() {
    Promise<int> p;
    Thread th([&] { p.SetValue(42); });               // any thread sets it, once
    int v = p.Get();                                  // a thread waits instead: 42
    th.Join();
    Promise<> done;                                   // a completion without a value
    done.SetValue();
    done.Get();                                       // set already: no wait
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <cstring>
#include <iostream>
#include <thread>

// A C library that does its work on a thread of its own and reports
// through a callback with a void* context: a Promise turns that into a
// co_await. The context is unmanaged memory (the library hands back a
// void*), so it holds the promise through a RootPtr.
using Completion = void (*)(void* context, int result);

void c_read_async(const char* text, char* buffer, Completion done, void* context) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));   // the I/O
        std::strcpy(buffer, text);
        done(context, (int)std::strlen(text));
    }).detach();
}

struct Context {
    RootPtr<Promise<int>> done;
};

void OnRead(void* context, int result) {
    auto ctx = static_cast<Context*>(context);
    ctx->done->SetValue(result);                              // from the library's thread: the task that awaits is made ready
    delete ctx;
}

Task<int> ReadLine(char* buffer) {
    Ptr<Promise<int>> done = Make<Promise<int>>();            // a managed object: the frame holds it, the context holds it too
    c_read_async("hello", buffer, OnRead, new Context{done});
    co_return co_await *done;                                 // suspended until OnRead, no thread held
}

int main() {
    char buffer[64];
    int n = Spawn(ReadLine(buffer)).Join();
    std::cout << "read " << n << " bytes: " << buffer << "\n";
    Scheduler::Stop();
    return n == 5 ? 0 : 1;
}
```

The output:

```
read 5 bytes: hello
```

## See also

- [SpawnBlocking](Blocking.md): a blocking call whose result comes back through a Promise; [Event](Event.md): the promise without a value; [Channel](Channel.md): what the wait is made of; [Select](Select.md), [Time](Time.md): bounding it
- [RootPtr](../Core/RootPtr.md): the promise held from unmanaged memory
- `tests/Sgcl/promise_and_blocking.cpp`: the behaviour above, checked.
