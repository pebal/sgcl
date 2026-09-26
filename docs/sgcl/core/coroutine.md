# sgcl::managed_frame, sgcl::frame_ptr

```cpp
#include "sgcl/core/coroutine.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    struct managed_frame;
    template<class Promise> class frame_ptr;
}
```

The frame of a C++20 coroutine, where its parameters, locals, temporaries and promise live between suspensions, is allocated with `operator new`: heap memory the collector does not see. A `tracked_ptr` in such a frame breaks rule 1 of [The rules](README.md#the-rules) and its object may be collected under it; debug builds assert it. `sgcl/core/coroutine.h` is the way out. A promise type that derives from `managed_frame` gets its frames from the managed heap instead, as buffers of words the collector traces conservatively, so everything the coroutine holds is a root for as long as the frame is held. The frame is held through a `frame_ptr<Promise>`: a [`root_ptr`](root_ptr.md) to the frame and the coroutine handle, move-only, that destroys the coroutine when destroyed. [`generator<T>`](generator.md) is a coroutine type built this way in the core; `async::task<T>` of the async module ([coroutine](../async/coroutine.md)) is another, which runs on the [scheduler](../async/scheduler.md) or by hand.

The buffer is four words longer than the frame, and the frame starts past them: a header that belongs to whoever drives the coroutine. The core gives it a length and no layout; the async module keeps a task's executor, its task-locals and the link of an executor's queue there ([executor](../async/executor.md), [task_local](../async/task_local.md)), and the generator, which runs where it is called, leaves it zero. The handle addresses the frame past the header, the frame's own pointers the buffer.

## How a frame becomes managed

The compiler looks the allocation function of a coroutine up in the scope of its promise type, so a promise that derives from `managed_frame` inherits its `operator new` and `operator delete`:

- `operator new` allocates the frame as a managed buffer of words (`detail::FrameWord[]`, the size rounded up to whole words, zeroed), in the state of an object a `unique_ptr` owns: a root already.
- `get_return_object` of the promise constructs a `frame_ptr` from the coroutine handle, which takes the frame over into a `root_ptr` and stores the frame's own `tracked_ptr` in the promise (`managed_frame::self`: what an awaiter copies to hold the frame while the coroutine waits on a channel, on a task or on the scheduler's queue). From then on the frame is an ordinary managed object, kept by whatever holds the `frame_ptr` or the waiting coroutine.
- `operator delete` runs when the coroutine is destroyed (`frame_ptr::destroy`, the destructor of `frame_ptr`), after the destructors of the coroutine's locals and promise. It frees a frame that no `frame_ptr` took over, an exception thrown before `get_return_object` for instance, and does nothing for a frame taken over: that memory is the collector's once nothing refers to it.

The collector traces a frame conservatively: every word that holds the address of a managed object keeps that object, and a word that holds data proves nothing about its offset, since the same offset is a pointer in one frame and data in another (the pointer maps of managed objects, which narrow by elimination, do not apply). The cost is the allocation of the frame as a managed buffer, a few tens of nanoseconds instead of `malloc`, and one conservative pass over the frame's words per cycle; a frame that holds no managed pointers costs that pass and nothing else. See [Coroutines](../async/README.md#coroutines) in the README.

## Rules

- A `frame_ptr` holds a `root_ptr`, and so does everything built on it: a `task`, a `generator`, a coroutine type of your own. It lives anywhere: on a stack, in a managed object, in another frame, in a `std::vector<async::task<int>>`, in a global. What it costs is a cell per handle ([root_ptr](root_ptr.md)), one per coroutine.
- The parameters, locals, temporaries and promise members of a coroutine whose promise derives from `managed_frame` are roots while the frame is held: a `tracked_ptr`, a container, a `task` held across a suspension all keep what they refer to. A waiting coroutine (on a channel, on a task, on the scheduler's queue) is held by what it waits on, so a detached task's frame lives while it runs. A promise that does not derive from `managed_frame` lives in `operator new` memory together with the rest of the frame, so neither it nor the coroutine's locals or parameters may hold a `tracked_ptr` (rule 1); the collector would not see the pointer, and the object could be collected while the coroutine is suspended.
- A `std::coroutine_handle` keeps nothing alive (rule 3). The handle a `frame_ptr` returns is valid while that `frame_ptr` holds the frame and no longer: once the `frame_ptr` is destroyed, moved from or `destroy()`ed, the frame's memory belongs to the collector. Destroy the coroutine through the `frame_ptr`, never through the handle: the `frame_ptr` would destroy it a second time.
- Destroying a `frame_ptr` destroys the coroutine at once, on the calling thread, which runs the destructors of its locals and promise, wherever they were suspended; the frame's memory is reclaimed by a later cycle. A `frame_ptr` moved out of a function takes its frame with it.
- A `frame_ptr` has no synchronization of its own: a coroutine is resumed or destroyed by one thread at a time, as with a `std::coroutine_handle`, and which thread that is does not matter to the collector (the scheduler's workers resume it on whichever is free).

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
struct Node { int value; tracked_ptr<Node> next; };

// A coroutine type of your own: the promise derives from managed_frame
class walker {
public:
    struct promise_type : managed_frame {
        tracked_ptr<Node> current;                      // in the frame: a root

        walker get_return_object() {
            return walker(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(tracked_ptr<Node> n) noexcept {
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
    const tracked_ptr<Node>& current() const { return _frame.promise().current; }

private:
    explicit walker(std::coroutine_handle<promise_type> h) : _frame(h) {}   // takes the frame over
    frame_ptr<promise_type> _frame;
};

walker walk(tracked_ptr<Node> head) {                   // the parameter: in the frame, a root
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
struct promise_type : managed_frame {
    my_coroutine get_return_object() {
        // the frame_ptr is made here, from the handle of this promise's coroutine
        return my_coroutine(frame_ptr<promise_type>(std::coroutine_handle<promise_type>::from_promise(*this)));
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
frame_ptr<P> f;                   // empty
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
Promise& async::promise() const;
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
frame_ptr<P> f;
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

#### release

```cpp
[[nodiscard]] handle_type release() noexcept;
```

Lets go of the frame without destroying the coroutine and returns its handle, as `unique_ptr::release` returns the pointer; the `frame_ptr` is empty afterwards. The coroutine goes on wherever it is and must see to its own end (a detached task destroys itself at its final suspend). The handle returned keeps nothing alive (rule 3): it stays valid while something else holds the frame, the coroutine's own scheduler entry for a detached task.

```cpp
auto h = f.release();                 // f empty, the coroutine not destroyed
assert(!f && h);
```

## See also

- [generator](generator.md): the coroutine type of the core built on a managed frame
- [coroutine](../async/coroutine.md): `task`, the coroutine type of the async module, and `async::generator`; [scheduler](../async/scheduler.md): what runs the tasks
- [tracked_ptr](tracked_ptr.md), [root_ptr](root_ptr.md), [unique_ptr](unique_ptr.md), [collector](collector.md)
- README: [Coroutines](../async/README.md#coroutines), [The rules](README.md#the-rules)
- `tests/async/coroutine.cpp` for the frame as a root: a local, a parameter, a frame in a managed object, a thousand suspended frames in a `sgcl::vector`
