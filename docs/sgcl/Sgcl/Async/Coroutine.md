# Sgcl::ManagedFrame, Sgcl::FramePtr, Sgcl::Task, Sgcl::Generator

```cpp
#include "sgcl/Sgcl/Async/Coroutine.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    using ManagedFrame = sgcl::managed_frame;   // the base of a promise whose frames are managed
    template<class P> class FramePtr;           // the owner of such a coroutine
    template<class T = void> class Task;        // a coroutine that runs on the scheduler or by hand
    template<class T> class Generator;          // a coroutine that yields values
}
```

The same classes in the `sgcl` interface: [managed_frame, frame_ptr, task, generator](../../async/coroutine.md). [Spawn, Go, Yield and the Scheduler](Scheduler.md) run the tasks, [WhenAll and WhenAny](When.md) compose them, an [AsyncGenerator](AsyncGenerator.md) is a generator that may wait.

The frame of a C++20 coroutine, where its parameters, locals, temporaries and promise live between suspensions, is allocated with `operator new`: heap memory the collector does not see. A `Ptr` in such a frame breaks rule 1 of [The rules](../../core/README.md#the-rules) and its object may be collected under it; debug builds assert it. `Coroutine.h` is the way out. A promise type that derives from `ManagedFrame` gets its frames from the managed heap instead, as buffers of words the collector traces conservatively, so everything the coroutine holds is a root for as long as the frame is held. The frame is held through a `FramePtr<P>`: a [`RootPtr`](../Core/RootPtr.md) to the frame and the coroutine handle, move-only, that destroys the coroutine when destroyed. `Task<T>` and `Generator<T>` are two coroutine types built this way; `Task` runs on the [scheduler](Scheduler.md) or by hand.

`Generator<T>` is used like `std::generator<T>` of C++23 (a range-for over the values a coroutine `co_yield`s), but it is a C++20 type with its frame on the managed heap, an input iterator, and nothing else. `Task<T>` is a coroutine that produces one value: `Spawn()` puts it on the scheduler's queue and a worker runs it, `Resume()` runs it by hand on the calling thread; a thread waits for it with `Join()`, a coroutine with `co_await task`, which suspends the awaiting coroutine until the task is done, no thread held meanwhile. The value type is always spelled, `Task<int>`, `Generator<Ptr<Node>>`.

## How a frame becomes managed

The compiler looks the allocation function of a coroutine up in the scope of its promise type, so a promise that derives from `ManagedFrame` inherits its `operator new` and `operator delete` (the promises of `Task` and `Generator` do):

- `operator new` allocates the frame as a managed buffer of words (the size rounded up to whole words, zeroed), in the state of an object a `UniquePtr` owns: a root already.
- `get_return_object` of the promise constructs a `FramePtr` from the coroutine handle, which takes the frame over into a `RootPtr` and stores the frame's own `Ptr` in the promise: what an awaiter copies to hold the frame while the coroutine waits on a channel, on a task or on the scheduler's queue. From then on the frame is an ordinary managed object, kept by whatever holds the `FramePtr` or the waiting coroutine.
- `operator delete` runs when the coroutine is destroyed (`FramePtr::Destroy`, the destructor of `FramePtr`), after the destructors of the coroutine's locals and promise. It frees a frame that no `FramePtr` took over, an exception thrown before `get_return_object` for instance, and does nothing for a frame taken over: that memory is the collector's once nothing refers to it.

The collector traces a frame conservatively: every word that holds the address of a managed object keeps that object, and a word that holds data proves nothing about its offset, since the same offset is a pointer in one frame and data in another (the pointer maps of managed objects, which narrow by elimination, do not apply). The cost is the allocation of the frame as a managed buffer, a few tens of nanoseconds instead of `malloc`, and one conservative pass over the frame's words per cycle; a frame that holds no managed pointers costs that pass and nothing else. See [Coroutines](../../async/README.md#coroutines) in the README.

## Rules

- A `FramePtr` holds a `RootPtr`, and so does everything built on it: a `Task`, a `Generator`, a coroutine type of your own. It lives anywhere: on a stack, in a managed object, in another frame, in a `std::vector<Task<int>>`, in a global. What it costs is a cell per handle ([RootPtr](../Core/RootPtr.md)), one per coroutine.
- The parameters, locals, temporaries and promise members of a coroutine whose promise derives from `ManagedFrame` are roots while the frame is held: a `Ptr`, a container, a `Task` held across a suspension all keep what they refer to. A waiting coroutine (on a channel, on a task, on the scheduler's queue) is held by what it waits on, so the frame of a task nobody holds lives while it runs. A promise that does not derive from `ManagedFrame` lives in `operator new` memory together with the rest of the frame, so neither it nor the coroutine's locals or parameters may hold a `Ptr` (rule 1); the collector would not see the pointer, and the object could be collected while the coroutine is suspended.
- A `std::coroutine_handle` keeps nothing alive (rule 3). The handle a `FramePtr` returns is valid while that `FramePtr` holds the frame and no longer: once the `FramePtr` is destroyed, moved from or `Destroy()`ed, the frame's memory belongs to the collector. Destroy the coroutine through the `FramePtr`, never through the handle: the `FramePtr` would destroy it a second time.
- Destroying a `FramePtr` destroys the coroutine at once, on the calling thread, which runs the destructors of its locals and promise, wherever they were suspended; the frame's memory is reclaimed by a later cycle. A `FramePtr` moved out of a function takes its frame with it.
- A `FramePtr` has no synchronization of its own: a coroutine is resumed or destroyed by one thread at a time, as with a `std::coroutine_handle`, and which thread that is does not matter to the collector (the scheduler's workers resume it on whichever is free). `Join()` and `IsDone()` of a task may be called from any thread. A task resumed by hand (`Resume()`) on its first run takes the resumer's executor and task-locals, as a task started on the scheduler takes its starter's. A task is awaited by one coroutine at a time (an assertion in debug builds); a task that ended holds nothing of its awaiter.

## Members

### ManagedFrame

```cpp
using ManagedFrame = sgcl::managed_frame;   // static operator new(size_t), static operator delete(void*, size_t)
```

The base of a promise type whose coroutines get their frames from the managed heap. Its `operator new` allocates a frame of `size` bytes as a managed buffer of words, its `operator delete` frees it if no `FramePtr` took it over and does nothing otherwise (see above). Neither is called by hand: the compiler calls them for every coroutine whose promise derives from `ManagedFrame`, with the size of the whole frame.

A promise of your own derives from `ManagedFrame` and returns, from `get_return_object`, an object that holds a `FramePtr<promise_type>` made from the handle. Everything else about the promise is ordinary C++20: `initial_suspend`, `final_suspend`, `return_value`/`return_void`, `yield_value`, `unhandled_exception`, and any members it needs, `Ptr` members included, since the promise lives in the frame:

```cpp
struct Node { int value; Ptr<Node> next; };

// A coroutine type of your own: the promise derives from ManagedFrame
class Walker {
public:
    struct promise_type : ManagedFrame {
        Ptr<Node> current;                                  // in the frame: a root

        Walker get_return_object() {
            return Walker(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(Ptr<Node> n) noexcept {
            current = n;                                    // co_yield: keep the node, suspend
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() { throw; }               // out of Step()
    };

    bool Step() {                                           // to the next co_yield: true, to the end: false
        _frame.Resume();
        return !_frame.IsDone();
    }
    const Ptr<Node>& Current() const { return _frame.Promise().current; }

private:
    explicit Walker(std::coroutine_handle<promise_type> h) : _frame(h) {}   // takes the frame over
    FramePtr<promise_type> _frame;
};

Walker Walk(Ptr<Node> head) {                               // the parameter: in the frame, a root
    for (auto n = head; n; n = n->next) {
        co_yield n;
    }
}
```

### FramePtr

```cpp
template<class P>
class FramePtr;

using PromiseType = P;
using HandleType = std::coroutine_handle<P>;
using InnerType = sgcl::frame_ptr<P>;
```

The owner of a coroutine whose promise derives from `ManagedFrame`: two words, a `RootPtr` to the frame and the coroutine handle. Move-only; destroys the coroutine when destroyed; `Release()` lets go of the frame without destroying the coroutine (a task detached). `Task` and `Generator` are each a single frame pointer and forward to it; a coroutine type of your own holds one the same way.

#### Constructors, assignment, destructor

```cpp
FramePtr() noexcept = default;
explicit FramePtr(HandleType h);
FramePtr(FramePtr&& o) noexcept;
FramePtr& operator=(FramePtr&& o) noexcept;
FramePtr(const FramePtr&) = delete;
FramePtr& operator=(const FramePtr&) = delete;
~FramePtr();
```

The default constructor makes an empty `FramePtr`: `false`, `IsDone()`. The constructor from a handle takes the coroutine's frame over: `h` must be the handle of a coroutine whose promise derives from `ManagedFrame`, and no other `FramePtr` may have taken that frame; the place to call it is the promise's `get_return_object`, with `std::coroutine_handle<P>::from_promise(*this)`. A move leaves the source empty; move assignment destroys the coroutine the target held first. The destructor destroys the coroutine, if any (see `Destroy`).

#### operator bool

```cpp
explicit operator bool() const noexcept;
```

`true` when the `FramePtr` holds a coroutine, `false` when empty (default-constructed, moved from, or after `Destroy()`).

#### Handle

```cpp
HandleType Handle() const noexcept;
```

The coroutine handle, null when empty. For the operations `FramePtr` does not wrap (`address()`, passing the handle to an awaiter of your own). The handle is valid while this `FramePtr` holds the frame; do not call `destroy()` on it, the `FramePtr` does that.

#### Promise

```cpp
P& Promise() const;
```

The coroutine's promise, the one in the frame. Precondition: not empty.

#### Resume

```cpp
void Resume();
```

Resumes the coroutine: it runs until it next suspends or ends. Precondition: not empty, suspended and not `IsDone()`, as for `std::coroutine_handle::resume`. An exception the coroutine lets out of its promise's `unhandled_exception` propagates from `Resume()`.

#### IsDone

```cpp
bool IsDone() const noexcept;
```

`true` when the `FramePtr` is empty or the coroutine is suspended at its final suspend point.

#### Destroy

```cpp
void Destroy() noexcept;
```

Destroys the coroutine, if any: runs the destructors of its locals and promise (wherever it was suspended), then lets go of the frame, whose memory the collector reclaims once nothing refers to it. The `FramePtr` is empty afterwards. The destructor and move assignment call it.

#### Release

```cpp
void Release() noexcept;
```

Lets go of the frame without destroying the coroutine: the coroutine runs on, or stays wherever it waits, held by what it waits on, and destroys its frame when it is done. What `Task::Detach` does. The `FramePtr` is empty afterwards.

#### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The frame pointer inside, as its own type.

### Task

```cpp
template<class T = void>
class Task;
```

A coroutine that produces one value and runs on the [scheduler](Scheduler.md) or by hand. `Spawn()` puts it on the queue of the ready and returns at once (so does the first `Join()` or `co_await` of a task nobody spawned: a task starts with the first of the three); a worker runs it to its next suspension, and the send, the task or the timer it waits for makes it ready again. `Resume()` runs it on the calling thread instead, a step at a time, as before the scheduler existed. What it `co_return`s is kept for `Result()`: a thread waits for it with `Join()`, a coroutine with `co_await task`, which suspends the awaiting coroutine until the task is done, with no thread held meanwhile; either rethrows what the task threw. `Task<void>` is the same without a value. A task dropped while it runs is detached (`Detach()`): its frame lives while it is queued, waiting or running, and is the collector's once it is done. Move-only, two words; a default-constructed `Task` is empty and `IsDone()`.

#### promise_type

```cpp
using promise_type = InnerType::promise_type;   // : managed frame
```

The promise of a `Task`. `initial_suspend` is `suspend_always`, so the coroutine body does not run until `Spawn()` or the first `Resume()`; at the final suspension the frame with its result stays until the `Task` is destroyed, the task is marked done (one word: running, done, or the handle of the coroutine awaiting it), a thread in `Join()` is woken, and a coroutine that `co_await`ed the task is handed to the scheduler. The value and an exception are kept; `Result()` reads them. A coroutine returning `Task<T>` may `co_await` a channel, another task, `Yield()`, or `std::suspend_always{}` (the plain "give control back to the resumer", for a task run by hand).

```cpp
Task<int> CountTo(int n) {         // nothing runs yet: the task is lazy
    int i = 0;
    while (i < n) {
        ++i;
        co_await Yield();          // to the back of the scheduler's queue; the frame keeps i
    }
    co_return i;
}
```

#### Constructor

```cpp
Task() noexcept;
```

An empty task: `IsDone()` is `true`, `Resume()`, `Join()` and `Result()` may not be called. A `Task` with a coroutine comes from calling a coroutine function that returns one; it is move constructible and move assignable, not copyable.

```cpp
Task<int> t;                       // empty, to be assigned
t = CountTo(3);                             // the coroutine, suspended before its first statement
```

#### Spawn

```cpp
Task& Spawn();                              // the member
template<class T> Task<T> Spawn(Task<T> t); // the free function: Spawn(f()) for `auto t = Spawn(f());`
```

Puts the task on the scheduler's queue and returns at once; a worker runs it. Once, before the task runs. The free function returns the task, for `auto t = Spawn(f());`. Both are `[[nodiscard]]`: a task object dropped destroys the coroutine, so a task nobody waits for is started with `Go(f())` instead.

```cpp
auto t = Spawn(CountTo(3));        // running on a worker, or about to
```

#### Resume

```cpp
void Resume();
```

Runs the coroutine on the calling thread to its next suspension or to its end, as a `std::coroutine_handle` would: for a task driven by hand, without the scheduler. Precondition: not empty, not `IsDone()`, not spawned. An exception the coroutine throws does not escape `Resume()`: the promise stores it and the task is `IsDone()`; `Result()` rethrows it.

```cpp
Task<int> Step(int n) {
    int i = 0;
    while (i < n) {
        ++i;
        co_await std::suspend_always{};     // back to the resumer
    }
    co_return i;
}
Task<int> t = Step(3);
t.Resume();                                 // i == 1, suspended
t.Resume();                                 // i == 2
```

#### IsDone

```cpp
bool IsDone() const noexcept;
```

Whether the coroutine has ended (or the task is empty). From any thread.

```cpp
auto t = Spawn(CountTo(3));
while (!t.IsDone()) {
    ThisThread::Yield();              // or Join(), which waits without spinning
}
```

#### Join

```cpp
T& Join();                                  // Task<T>
void Join();                                // Task<void>
```

Waits, on the calling thread, until the task is done, and returns `Result()`. A task nobody started yet is put on the scheduler first: `CountTo(3).Join()` is `Spawn(CountTo(3)).Join()`. For a thread; not from a task on a worker, which would block the worker (`co_await` the task there; debug builds assert it).

```cpp
int n = Spawn(CountTo(3)).Join();  // 3, or the exception the coroutine threw
```

#### co_await task

```cpp
auto operator co_await() noexcept;          // co_await t: T, or void; rethrows
```

From a coroutine with a managed frame: suspends it until the task is done, with no thread held, and gives the result. A task nobody started yet is put on the scheduler first, so `co_await CountTo(3)` runs the task on a worker and waits for it; a task that is done already does not suspend. One coroutine may await a task at a time; the same coroutine may await it again after.

```cpp
Task<int> Twice() {
    int a = co_await Spawn(CountTo(3));
    int b = co_await Spawn(CountTo(4));
    co_return a + b;                        // 7
}
```

#### Result

```cpp
T& Result();                                // Task<T>
void Result();                              // Task<void>
```

The value the coroutine `co_return`ed, a reference into the frame that is valid while the task holds it; for `Task<void>`, nothing. If the coroutine ended with an exception, `Result()` rethrows it, every time it is called. Precondition: not empty and `IsDone()` (the value of a coroutine that has not returned yet does not exist).

```cpp
auto t = Spawn(CountTo(3));
t.Join();
int n = t.Result();                         // 3, again
```

#### Detach

```cpp
void Detach() noexcept;
```

Lets go of the task: it runs on, or stays wherever it waits, and destroys its frame when it is done (the locals and parameters with it: a task it awaited, a `RootPtr` it held, released), the memory the collector's from then on; a task detached after it is done is destroyed at once. The task object is empty after. For a task whose result nobody needs; `Go(F())` is a spawn and a detach in one. A task detached before anyone started it never runs: its frame is left to the collector as it is, its locals never destroyed.

```cpp
Task<> LogForever(Channel<String>& queue);   // a task that never ends
Channel<String> queue;
auto t = Spawn(LogForever(queue));
t.Detach();                                 // the same as Go(LogForever(queue));
```

#### Go

```cpp
template<class T> void Go(Task<T> t);
```

Puts the task on the scheduler and lets go of it, Go's `go f()`: `t.Spawn().Detach()`. For a task nobody waits for; the handle of one somebody does is kept from `Spawn`.

```cpp
Task<> LogForever(Channel<String>& queue);
Channel<String> queue;
Go(LogForever(queue));
```

#### Destroy

```cpp
void Destroy() noexcept;
```

Destroys the coroutine, running the destructors of its locals and promise, and leaves the task empty; the frame's memory goes to the collector. For a task that never ran or is done, never for one that is queued or waiting (detach it instead). The task's destructor does the same.

```cpp
Task<int> t = CountTo(3);
t.Resume();
t.Destroy();                                // the frame's locals are gone; t is empty and IsDone()
```

#### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The task inside, as its own type.

### Generator

```cpp
template<class T>
class Generator;
```

A coroutine that `co_yield`s values, consumed with a range-for or with `Next()`/`Value()`; an exception it throws comes out of `Next()` (or the iterator's `++`, or `begin`). Move-only, two words; a default-constructed `Generator` is empty and yields nothing. Single pass: the coroutine advances with every `Next()`, and `begin` advances it too.

#### promise_type

```cpp
using promise_type = InnerType::promise_type;   // : managed frame
```

The promise of a `Generator`. Lazy like `Task`: nothing runs until the first `Next()` (or `begin`). `co_yield v` moves `v` into the promise and suspends; the coroutine ends with `co_return;` or by falling off its end, and may not `co_return` a value.

```cpp
Generator<int> Squares(int n) {
    for (int i : Range(1, n + 1)) {
        co_yield i * i;                         // value, then suspended until the next Next()
    }
}
```

#### Constructor

```cpp
Generator() noexcept;
```

An empty generator: `Next()` returns `false`, `begin(g) == end(g)`, `Value()` may not be called. A generator with a coroutine comes from calling a coroutine function that returns one; move constructible and move assignable, not copyable.

```cpp
Generator<int> g;                      // empty: no values
g = Squares(4);                                 // the coroutine, not started yet
```

#### Next

```cpp
bool Next();
```

Runs the coroutine to its next `co_yield`: `true`, and `Value()` is the yielded value; or to its end: `false`. On an empty or finished generator, `false` at once. An exception the coroutine throws is rethrown by `Next()`, after which the generator is finished.

```cpp
Generator<int> g = Squares(4);
while (g.Next()) {
    int v = g.Value();                          // 1, 4, 9, 16
}
```

#### Value

```cpp
const T& Value() const noexcept;
```

The value of the last `co_yield`, a reference into the frame: overwritten by the next `co_yield`, gone when the generator is destroyed. Precondition: the last `Next()` returned `true`.

```cpp
Generator<int> g = Squares(4);
if (g.Next()) {
    const int& first = g.Value();               // 1
}
```

#### begin, end

```cpp
auto begin(Generator&);                         // free functions: an input iterator over the values
auto end(Generator&) noexcept;
```

The range-for support. `begin(g)` calls `Next()`, so it runs the coroutine to its first value and returns an iterator to it, or the end when there is none; it is not repeatable: a second `begin` advances the coroutine again. Dereferencing the iterator gives the current value as `const T&`; `++` runs the coroutine to its next `co_yield` and turns into the end when the coroutine ends. An exception the coroutine throws comes out of `++`.

```cpp
for (int v : Squares(4)) {                      // the temporary generator lives for the loop
    // v: 1, 4, 9, 16
}
```

#### Destroy

```cpp
void Destroy() noexcept;
```

Destroys the coroutine, running the destructors of its locals and promise, and leaves the generator empty; the destructor does the same. A generator abandoned in the middle of its values is destroyed the same way, wherever it was suspended.

```cpp
Generator<int> g = Squares(100);
g.Next();
g.Destroy();                                    // the other 99 never happen; g is empty
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"

#include <coroutine>
#include <iostream>

struct Node {
    Node(int v, Ptr<Node> n) : value(v), next(n) {}
    int value;
    Ptr<Node> next;
};

// Yields the nodes of a chain it builds as it goes: the local keeps
// the whole chain alive while the generator is suspended
Generator<Ptr<Node>> Chain(int count) {
    Ptr<Node> last;                            // a local in a managed frame: a root
    for (int i : Range(1, count + 1)) {
        Ptr n = Make<Node>(i, last);
        last = n;
        co_yield n;
    }
}

// Sums a chain one node per resume: the parameter copy in the frame keeps the head
Task<int> Sum(Ptr<Node> head) {
    int s = 0;
    for (auto n = head; n; n = n->next) {
        s += n->value;
        co_await std::suspend_always{};                 // back to the caller, the chain stays alive
    }
    co_return s;
}

int main() {
    Ptr<Node> head;
    for (auto& n : Chain(4)) {                          // the generator's frame holds the chain
        head = n;                                       // the last one yielded is the head
    }
    Collector::Collect();                      // optional, only to show the point at once: the
                                                        // generator is gone, head keeps the chain
    Task<int> t = Sum(head);
    head = nullptr;                                     // the task's frame is the only root now
    while (!t.IsDone()) {
        t.Resume();
        Collector::Collect();                  // optional: the chain survives every cycle
    }
    std::cout << t.Result() << '\n';                    // 10
}
```

The output:

```
10
```

## See also

- [Scheduler](Scheduler.md): `Spawn`, `Go`, `Yield`, what runs the tasks; [AsyncGenerator](AsyncGenerator.md): a generator that may wait; [WhenAll, WhenAny](When.md): the composition of tasks; [Channel](Channel.md): what they wait on
- [Ptr](../Core/Ptr.md), [RootPtr](../Core/RootPtr.md), [List](../Containers/List.md) (a `List<Task<T>>` holds many tasks)
- [Collector](../Core/Collector.md), [config](../../core/config.md)
- README: [Coroutines](../../async/README.md#coroutines), [The rules](../../core/README.md#the-rules), [Stack roots](../../../garbage_collector/overview.md#stack-roots), [Threads](../../async/README.md#threads)
