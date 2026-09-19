# sgcl::managed_frame, sgcl::frame_ptr, sgcl::task, sgcl::generator

```cpp
#include "sgcl/sgcl.h"          // or "sgcl/async/coroutine.h"

namespace sgcl {
    struct managed_frame;
    template<class Promise> class frame_ptr;
    template<class T = void> class task;
    template<class T> class generator;
}
```

The same classes in the `Sgcl` interface: [ManagedFrame, FramePtr, Task, Generator](../Sgcl/Async/Coroutine.md).

The frame of a C++20 coroutine, where its parameters, locals, temporaries and promise live between suspensions, is allocated with `operator new`: heap memory the collector does not see. A `tracked_ptr` in such a frame breaks rule 1 of [The rules](../core/README.md#the-rules) and its object may be collected under it; debug builds assert it. `sgcl/async/coroutine.h` is the way out. A promise type that derives from `managed_frame` gets its frames from the managed heap instead, as buffers of words the collector traces conservatively, so everything the coroutine holds is a root for as long as the frame is held. The frame is held through a `frame_ptr<Promise>`: a [`root_ptr`](../core/root_ptr.md) to the frame and the coroutine handle, move-only, that destroys the coroutine when destroyed. `task<T>` and `generator<T>` are two coroutine types built this way; `task` runs on the [scheduler](scheduler.md) or by hand.

`generator<T>` is used like `std::generator<T>` of C++23 (a range-for over the values a coroutine `co_yield`s), but it is a C++20 type with its frame on the managed heap, an input iterator, and nothing else. `task<T>` is a coroutine that produces one value: `spawn()` puts it on the scheduler's queue and a worker runs it, `resume()` runs it by hand on the calling thread; a thread waits for it with `join()`, a coroutine with `co_await task`, which suspends the awaiting coroutine until the task is done, no thread held meanwhile. The value type is always spelled, `task<int>`, `generator<tracked_ptr<Node>>`.

## How a frame becomes managed

The compiler looks the allocation function of a coroutine up in the scope of its promise type, so a promise that derives from `managed_frame` inherits its `operator new` and `operator delete`:

- `operator new` allocates the frame as a managed buffer of words (`detail::FrameWord[]`, the size rounded up to whole words, zeroed), in the state of an object a `unique_ptr` owns: a root already.
- `get_return_object` of the promise constructs a `frame_ptr` from the coroutine handle, which takes the frame over into a `root_ptr` and stores the frame's own `tracked_ptr` in the promise (`managed_frame::self`: what an awaiter copies to hold the frame while the coroutine waits on a channel, on a task or on the scheduler's queue). From then on the frame is an ordinary managed object, kept by whatever holds the `frame_ptr` or the waiting coroutine.
- `operator delete` runs when the coroutine is destroyed (`frame_ptr::destroy`, the destructor of `frame_ptr`), after the destructors of the coroutine's locals and promise. It frees a frame that no `frame_ptr` took over, an exception thrown before `get_return_object` for instance, and does nothing for a frame taken over: that memory is the collector's once nothing refers to it.

The collector traces a frame conservatively: every word that holds the address of a managed object keeps that object, and a word that holds data proves nothing about its offset, since the same offset is a pointer in one frame and data in another (the pointer maps of managed objects, which narrow by elimination, do not apply). The cost is the allocation of the frame as a managed buffer, a few tens of nanoseconds instead of `malloc`, and one conservative pass over the frame's words per cycle; a frame that holds no managed pointers costs that pass and nothing else. See [Coroutines](README.md#coroutines) in the README.

## Rules

- A `frame_ptr` holds a `root_ptr`, and so does everything built on it: a `task`, a `generator`, a coroutine type of your own. It lives anywhere: on a stack, in a managed object, in another frame, in a `std::vector<task<int>>`, in a global. What it costs is a cell per handle ([root_ptr](../core/root_ptr.md)), one per coroutine.
- The parameters, locals, temporaries and promise members of a coroutine whose promise derives from `managed_frame` are roots while the frame is held: a `tracked_ptr`, a container, a `task` held across a suspension all keep what they refer to. A waiting coroutine (on a channel, on a task, on the scheduler's queue) is held by what it waits on, so a detached task's frame lives while it runs. A promise that does not derive from `managed_frame` lives in `operator new` memory together with the rest of the frame, so neither it nor the coroutine's locals or parameters may hold a `tracked_ptr` (rule 1); the collector would not see the pointer, and the object could be collected while the coroutine is suspended.
- A `std::coroutine_handle` keeps nothing alive (rule 3). The handle a `frame_ptr` returns is valid while that `frame_ptr` holds the frame and no longer: once the `frame_ptr` is destroyed, moved from or `destroy()`ed, the frame's memory belongs to the collector. Destroy the coroutine through the `frame_ptr`, never through the handle: the `frame_ptr` would destroy it a second time.
- Destroying a `frame_ptr` destroys the coroutine at once, on the calling thread, which runs the destructors of its locals and promise, wherever they were suspended; the frame's memory is reclaimed by a later cycle. A `frame_ptr` moved out of a function takes its frame with it.
- A `frame_ptr` has no synchronization of its own: a coroutine is resumed or destroyed by one thread at a time, as with a `std::coroutine_handle`, and which thread that is does not matter to the collector (the scheduler's workers resume it on whichever is free). `join()` and `done()` of a task may be called from any thread. A task resumed by hand (`resume()`) on its first run takes the resumer's executor and task-locals, as a task started on the scheduler takes its starter's. A task is awaited by one coroutine at a time (an assertion in debug builds); a task that ended holds nothing of its awaiter.

## Members

### managed_frame

```cpp
struct managed_frame {
    static void* operator new(size_t size);
    static void operator delete(void* p, size_t) noexcept;
};
```

The base of a promise type whose coroutines get their frames from the managed heap. `operator new` allocates a frame of `size` bytes as a managed buffer of words, `operator delete` frees it if no `frame_ptr` took it over and does nothing otherwise (see above). Neither is called by hand: the compiler calls them for every coroutine whose promise derives from `managed_frame`, with the size of the whole frame.

A promise of your own derives from `managed_frame` and returns, from `get_return_object`, an object that holds a `frame_ptr<promise_type>` made from the handle. Everything else about the promise is ordinary C++20: `initial_suspend`, `final_suspend`, `return_value`/`return_void`, `yield_value`, `unhandled_exception`, and any members it needs, `tracked_ptr` members included, since the promise lives in the frame:

```cpp
struct Node { int value; sgcl::tracked_ptr<Node> next; };

// A coroutine type of your own: the promise derives from managed_frame
class walker {
public:
    struct promise_type : sgcl::managed_frame {
        sgcl::tracked_ptr<Node> current;                      // in the frame: a root

        walker get_return_object() {
            return walker(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(sgcl::tracked_ptr<Node> n) noexcept {
            current = n;                                    // co_yield: keep the node, suspend
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() { throw; }               // out of step()
    };

    bool step() {                                           // to the next co_yield: true, to the end: false
        _frame.resume();
        return !_frame.done();
    }
    const sgcl::tracked_ptr<Node>& current() const { return _frame.promise().current; }

private:
    explicit walker(std::coroutine_handle<promise_type> h) : _frame(h) {}   // takes the frame over
    sgcl::frame_ptr<promise_type> _frame;
};

walker walk(sgcl::tracked_ptr<Node> head) {                   // the parameter: in the frame, a root
    for (auto n = head; n; n = n->next) {
        co_yield n;
    }
}
```

### frame_ptr

```cpp
template<class Promise>
class frame_ptr;

using promise_type = Promise;
using handle_type = std::coroutine_handle<Promise>;
```

The owner of a coroutine whose promise derives from `managed_frame`: two words, a `root_ptr` to the frame and the coroutine handle. Move-only; destroys the coroutine when destroyed; `release()` lets go of the frame without destroying the coroutine (a task detached). `task` and `generator` are each a single `frame_ptr` and forward to it; a coroutine type of your own holds one the same way.

#### Constructors, assignment, destructor

```cpp
frame_ptr() noexcept = default;
explicit frame_ptr(handle_type h);
frame_ptr(frame_ptr&& o) noexcept;
frame_ptr& operator=(frame_ptr&& o) noexcept;
frame_ptr(const frame_ptr&) = delete;
frame_ptr& operator=(const frame_ptr&) = delete;
~frame_ptr();
```

The default constructor makes an empty `frame_ptr`: `false`, `done()`. The constructor from a handle takes the coroutine's frame over: `h` must be the handle of a coroutine whose promise derives from `managed_frame`, and no other `frame_ptr` may have taken that frame; the place to call it is the promise's `get_return_object`, with `std::coroutine_handle<Promise>::from_promise(*this)`. A move leaves the source empty; move assignment destroys the coroutine the target held first. The destructor destroys the coroutine, if any (see `destroy`).

```cpp
struct promise_type : sgcl::managed_frame {
    my_coroutine get_return_object() {
        // the frame_ptr is made here, from the handle of this promise's coroutine
        return my_coroutine(sgcl::frame_ptr<promise_type>(std::coroutine_handle<promise_type>::from_promise(*this)));
    }
    // ...
};
```

#### operator bool

```cpp
explicit operator bool() const noexcept;
```

`true` when the `frame_ptr` holds a coroutine, `false` when empty (default-constructed, moved from, or after `destroy()`).

```cpp
sgcl::frame_ptr<P> f;                   // empty
if (!f) { /* nothing to resume */ }
```

#### handle

```cpp
handle_type handle() const noexcept;
```

The coroutine handle, null when empty. For the operations `frame_ptr` does not wrap (`address()`, passing the handle to an awaiter of your own). The handle is valid while this `frame_ptr` holds the frame; do not call `destroy()` on it, the `frame_ptr` does that.

```cpp
std::coroutine_handle<P> h = f.handle();
void* frame = h.address();            // the managed buffer the frame lives in
```

#### promise

```cpp
Promise& promise() const;
```

The coroutine's promise, the one in the frame. Precondition: not empty.

```cpp
if (f) {
    P& p = f.promise();               // the members the promise keeps (a value, an error, ...)
}
```

#### resume

```cpp
void resume();
```

Resumes the coroutine: it runs until it next suspends or ends. Precondition: not empty, suspended and not `done()`, as for `std::coroutine_handle::resume`. An exception the coroutine lets out of its promise's `unhandled_exception` propagates from `resume()`.

```cpp
while (!f.done()) {
    f.resume();                       // one step per resume, whatever the coroutine co_awaits
}
```

#### done

```cpp
bool done() const noexcept;
```

`true` when the `frame_ptr` is empty or the coroutine is suspended at its final suspend point.

```cpp
sgcl::frame_ptr<P> f;
bool d = f.done();                    // true: nothing to resume
```

#### destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine, if any: runs the destructors of its locals and promise (wherever it was suspended; `std::coroutine_handle::destroy`), then lets go of the frame, whose memory the collector reclaims once nothing refers to it. The `frame_ptr` is empty afterwards. The destructor and move assignment call it; calling it early releases what the frame holds before the `frame_ptr` goes out of scope.

```cpp
f.destroy();                          // the coroutine's locals are gone now
assert(!f && f.done());
```

### task

```cpp
template<class T = void>
class task;
```

A coroutine that produces one value and runs on the [scheduler](scheduler.md) or by hand. `spawn()` puts it on the queue of the ready and returns at once (so does the first `join()` or `co_await` of a task nobody spawned: a task starts with the first of the three); a worker runs it to its next suspension, and the send, the task or the timer it waits for makes it ready again. `resume()` runs it on the calling thread instead, a step at a time, as before the scheduler existed. What it `co_return`s is kept for `result()`: a thread waits for it with `join()`, a coroutine with `co_await task`, which suspends the awaiting coroutine until the task is done, with no thread held meanwhile; either rethrows what the task threw. `task<void>` is the same without a value. A task dropped while it runs is detached (`detach()`): its frame lives while it is queued, waiting or running, and is the collector's once it is done. Move-only, two words (a `frame_ptr<promise_type>`); a default-constructed `task` is empty and `done()`.

#### promise_type

```cpp
struct promise_type : detail::TaskPromiseBase {   // : managed_frame
    std::optional<T> value;               // task<T> only
    std::exception_ptr error;

    task get_return_object();
    std::suspend_always initial_suspend() noexcept;
    /* final awaiter */ final_suspend() noexcept;   // marks the task done, wakes a join() that waits (none waiting: no notify, which was a fetch-add on a shared table per task end), hands the awaiting coroutine to the scheduler
    void return_value(T v);               // task<T>: value.emplace(std::move(v))
    void return_void() noexcept;          // task<void>
    void unhandled_exception() noexcept;  // error = std::current_exception()
};
```

The promise of a `task`. `initial_suspend` is `suspend_always`, so the coroutine body does not run until `spawn()` or the first `resume()`; at the final suspension the frame with its result stays until the `task` is destroyed, the task is marked done (one word: running, done, or the handle of the coroutine awaiting it), a thread in `join()` is woken, and a coroutine that `co_await`ed the task is handed to the scheduler. The value goes into `value`, an exception into `error`; `result()` reads them. A coroutine returning `task<T>` may `co_await` a channel, another task, `sgcl::yield()`, or `std::suspend_always{}` (the plain "give control back to the resumer", for a task run by hand).

```cpp
sgcl::task<int> count_to(int n) {           // nothing runs yet: the task is lazy
    int i = 0;
    while (i < n) {
        ++i;
        co_await sgcl::yield();           // to the back of the scheduler's queue; the frame keeps i
    }
    co_return i;
}
```

#### Constructor

```cpp
task() noexcept = default;
```

An empty task: `done()` is `true`, `resume()`, `join()` and `result()` may not be called. A `task` with a coroutine comes from calling a coroutine function that returns one; it is move constructible and move assignable, not copyable.

```cpp
sgcl::task<int> t;                          // empty, to be assigned
t = count_to(3);                          // the coroutine, suspended before its first statement
```

#### spawn

```cpp
task& spawn();                            // the member
template<class T> task<T> spawn(task<T> t);   // the free function: spawn(f()) for `auto t = spawn(f());`
```

Puts the task on the scheduler's queue and returns at once; a worker runs it. Once, before the task runs. The free function returns the task, for `auto t = sgcl::spawn(f());`. Both are `[[nodiscard]]`: a task object dropped destroys the coroutine, so a task nobody waits for is started with `sgcl::go(f())` instead.

```cpp
auto t = sgcl::spawn(count_to(3));        // running on a worker, or about to
```

#### resume

```cpp
void resume();
```

Runs the coroutine on the calling thread to its next suspension or to its end, as a `std::coroutine_handle` would: for a task driven by hand, without the scheduler. Precondition: not empty, not `done()`, not spawned. An exception the coroutine throws does not escape `resume()`: the promise stores it and the task is `done()`; `result()` rethrows it.

```cpp
sgcl::task<int> step(int n) {
    int i = 0;
    while (i < n) {
        ++i;
        co_await std::suspend_always{};   // back to the resumer
    }
    co_return i;
}
sgcl::task<int> t = step(3);
t.resume();                               // i == 1, suspended
t.resume();                               // i == 2
```

#### done

```cpp
bool done() const noexcept;
```

Whether the coroutine has ended (or the task is empty). From any thread.

```cpp
while (!t.done()) {
    sgcl::this_thread::yield();            // or join(), which waits without spinning
}
```

#### join

```cpp
T& join();                                // task<T>
void join();                              // task<void>
```

Waits, on the calling thread, until the task is done, and returns `result()`. A task nobody started yet is put on the scheduler first: `count_to(3).join()` is `spawn(count_to(3)).join()`. For a thread; not from a task on a worker, which would block the worker (`co_await` the task there; debug builds assert it).

```cpp
int n = sgcl::spawn(count_to(3)).join();   // 3, or the exception the coroutine threw
```

#### co_await task

```cpp
awaiter operator co_await() noexcept;    // co_await t: T, or void; rethrows
```

From a coroutine with a managed frame: suspends it until the task is done, with no thread held, and gives the result. A task nobody started yet is put on the scheduler first, so `co_await count_to(3)` runs the task on a worker and waits for it; a task that is done already does not suspend. One coroutine may await a task at a time; the same coroutine may await it again after.

```cpp
sgcl::task<int> twice() {
    int a = co_await sgcl::spawn(count_to(3));
    int b = co_await sgcl::spawn(count_to(4));
    co_return a + b;                      // 7
}
```

#### result

```cpp
T& result();                              // task<T>
void result();                            // task<void>
```

The value the coroutine `co_return`ed, a reference into the frame that is valid while the task holds it; for `task<void>`, nothing. If the coroutine ended with an exception, `result()` rethrows it, every time it is called. Precondition: not empty and `done()` (the value of a coroutine that has not returned yet does not exist).

```cpp
auto t = sgcl::spawn(count_to(3));
t.join();
int n = t.result();                       // 3, again
```

#### detach

```cpp
void detach() noexcept;
```

Lets go of the task: it runs on, or stays wherever it waits, and destroys its frame when it is done (the locals and parameters with it: a task it awaited, a `root_ptr` it held, released), the memory the collector's from then on; a task detached after it is done is destroyed at once. The task object is empty after. For a task whose result nobody needs; `sgcl::go(f())` is a spawn and a detach in one. A task detached before anyone started it never runs: its frame is left to the collector as it is, its locals never destroyed.

```cpp
auto t = sgcl::spawn(log_forever(queue));
t.detach();                               // the same as sgcl::go(log_forever(queue));
```

#### go

```cpp
template<class T> void go(task<T> t);
```

Puts the task on the scheduler and lets go of it, Go's `go f()`: `t.spawn().detach()`. For a task nobody waits for; the handle of one somebody does is kept from `sgcl::spawn`.

```cpp
sgcl::go(log_forever(queue));
```

#### destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine, running the destructors of its locals and promise, and leaves the task empty; the frame's memory goes to the collector. For a task that never ran or is done, never for one that is queued or waiting (detach it instead). The task's destructor does the same.

```cpp
sgcl::task<int> t = count_to(3);
t.resume();
t.destroy();                              // the frame's locals are gone; t is empty and done()
```

### generator

```cpp
template<class T>
class generator;
```

A coroutine that `co_yield`s values, consumed with a range-for or with `next()`/`value()`; an exception it throws comes out of `next()` (or the iterator's `++`, or `begin()`). Move-only, two words (a `frame_ptr<promise_type>`); a default-constructed `generator` is empty and yields nothing. Single pass: the coroutine advances with every `next()`, and `begin()` advances it too.

#### promise_type

```cpp
struct promise_type : managed_frame {
    std::optional<T> value;
    std::exception_ptr error;

    generator get_return_object();
    std::suspend_always initial_suspend() noexcept;
    std::suspend_always final_suspend() noexcept;
    std::suspend_always yield_value(T v);     // value.emplace(std::move(v)), then suspend
    void return_void() noexcept;
    void unhandled_exception() noexcept;      // error = std::current_exception()
};
```

The promise of a `generator`. Lazy like `task`: nothing runs until the first `next()` (or `begin()`). `co_yield v` moves `v` into `value` and suspends; the coroutine ends with `co_return;` or by falling off its end, and may not `co_return` a value.

```cpp
sgcl::generator<int> squares(int n) {
    for (int i : sgcl::range(1, n + 1)) {
        co_yield i * i;                       // value, then suspended until the next next()
    }
}
```

#### iterator

```cpp
class iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

    iterator() noexcept = default;
    reference operator*() const noexcept;     // the generator's value()
    pointer operator->() const noexcept;
    iterator& operator++();                   // next(); equal to end() when it returns false
    void operator++(int);
    bool operator==(const iterator& o) const noexcept;
    bool operator!=(const iterator& o) const noexcept;
};
```

An input iterator over the values, for the range-for. Dereferencing gives the current value as `const T&`; `++` runs the coroutine to its next `co_yield` and turns into `end()` when the coroutine ends. Post-increment returns nothing. Two iterators are equal when they refer to the same generator or are both `end()`. An exception the coroutine throws comes out of `++`.

```cpp
sgcl::generator<int> g = squares(3);
for (auto it = g.begin(); it != g.end(); ++it) {
    int v = *it;                              // 1, 4, 9
}
```

#### Constructor

```cpp
generator() noexcept = default;
```

An empty generator: `next()` returns `false`, `begin() == end()`, `value()` may not be called. A generator with a coroutine comes from calling a coroutine function that returns one; move constructible and move assignable, not copyable.

```cpp
sgcl::generator<int> g;                         // empty: no values
g = squares(4);                               // the coroutine, not started yet
```

#### next

```cpp
bool next();
```

Runs the coroutine to its next `co_yield`: `true`, and `value()` is the yielded value; or to its end: `false`. On an empty or finished generator, `false` at once. An exception the coroutine throws is rethrown by `next()`, after which the generator is finished.

```cpp
sgcl::generator<int> g = squares(4);
while (g.next()) {
    int v = g.value();                        // 1, 4, 9, 16
}
```

#### value

```cpp
const T& value() const noexcept;
```

The value of the last `co_yield`, a reference into the frame (the promise's `value`): overwritten by the next `co_yield`, gone when the generator is destroyed. Precondition: the last `next()` returned `true`.

```cpp
sgcl::generator<int> g = squares(4);
if (g.next()) {
    const int& first = g.value();             // 1
}
```

#### begin, end

```cpp
iterator begin();
iterator end() noexcept;
```

The range-for support. `begin()` calls `next()`, so it runs the coroutine to its first value and returns an iterator to it, or `end()` when there is none; it is not repeatable: a second `begin()` advances the coroutine again. `end()` is the empty iterator.

```cpp
for (int v : squares(4)) {                    // the temporary generator lives for the loop
    // v: 1, 4, 9, 16
}
```

#### destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine, running the destructors of its locals and promise, and leaves the generator empty; the destructor does the same. A generator abandoned in the middle of its values is destroyed the same way, wherever it was suspended.

```cpp
sgcl::generator<int> g = squares(100);
g.next();
g.destroy();                                  // the other 99 never happen; g is empty
```

### async_generator

```cpp
#include "sgcl/async/async_generator.h"   // or "sgcl/sgcl.h"

template<class T>
class async_generator;

next_op next() noexcept;                  // co_await g.next(): std::optional<T>, nothing at the end; rethrows
bool done() const noexcept;
```

A generator that may wait: a coroutine that `co_yield`s values and `co_await`s between them (a channel, a sleep, a task), consumed from a task with `while (auto v = co_await g.next())`. The consumer and the generator hand control to each other directly, without the scheduler's queue: `next()` resumes the generator on the consumer's worker, a `co_yield` resumes the consumer where the generator is, and while the generator waits for something the consumer waits with it, no thread held by either. The generator runs as part of its consumer: at every `next()` its frame takes the consumer's executor and task-locals, so a wait of its own resumes it where the consumer runs (an [executor](executor.md), a strand) and the functions under it, and the consumer resumed by the yield, see the consumer's [task-locals](task_local.md); a local the generator sets itself lasts until its next yield. Both frames are on the managed heap: the generator's held by the `async_generator` object, the consumer's by the generator's promise while it waits, and a value yielded is held by the promise until the consumer takes it. Move-only; single pass; `next()` past the end gives nothing again; an exception the generator throws comes out of the `next()` that ran into it.

```cpp
sgcl::async_generator<int> tens(sgcl::channel<int>& in) {
    while (auto v = co_await in.async_receive()) {    // waits between yields
        co_yield *v * 10;
    }
}

sgcl::task<int> consume(sgcl::channel<int>& in) {
    auto g = tens(in);
    int sum = 0;
    while (auto v = co_await g.next()) {              // nothing once in is closed and drained
        sum += *v;
    }
    co_return sum;
}
```

## Example

```cpp
#include "sgcl/sgcl.h"

#include <coroutine>
#include <iostream>

struct Node {
    Node(int v, sgcl::tracked_ptr<Node> n) : value(v), next(n) {}
    int value;
    sgcl::tracked_ptr<Node> next;
};

// Yields the nodes of a chain it builds as it goes: the local keeps
// the whole chain alive while the generator is suspended
sgcl::generator<sgcl::tracked_ptr<Node>> chain(int count) {
    sgcl::tracked_ptr<Node> last;                         // a local in a managed frame: a root
    for (int i : sgcl::range(1, count + 1)) {
        sgcl::tracked_ptr n = sgcl::make_tracked<Node>(i, last);
        last = n;
        co_yield n;
    }
}

// Sums a chain one node per resume: the parameter copy in the frame keeps the head
sgcl::task<int> sum(sgcl::tracked_ptr<Node> head) {
    int s = 0;
    for (auto n = head; n; n = n->next) {
        s += n->value;
        co_await std::suspend_always{};                 // back to the caller, the chain stays alive
    }
    co_return s;
}

int main() {
    sgcl::tracked_ptr<Node> head;
    for (auto& n : chain(4)) {                          // the generator's frame holds the chain
        head = n;                                       // the last one yielded is the head
    }
    sgcl::collector::force_collect();                     // optional, only to show the point at once: the
                                                        // generator is gone, head keeps the chain
    sgcl::task<int> t = sum(head);
    head = nullptr;                                     // the task's frame is the only root now
    while (!t.done()) {
        t.resume();
        sgcl::collector::force_collect();                 // optional: the chain survives every cycle
    }
    std::cout << t.result() << '\n';                    // 10
}
```

The output:

```
10
```

## See also

- [tracked_ptr](../core/tracked_ptr.md), [unique_ptr](../core/unique_ptr.md), [vector](../containers/vector.md) (a `sgcl::vector<task<T>>` holds many tasks)
- [collector](../core/collector.md), [config](../core/config.md)
- README: [Coroutines](README.md#coroutines), [The rules](../core/README.md#the-rules), [Stack roots](../../garbage_collector/overview.md#stack-roots), [Threads](README.md#threads)
- `tests/async/coroutine.cpp` for the frame as a root: a local, a parameter, a frame in a managed object, a thousand suspended frames in a `sgcl::vector`
