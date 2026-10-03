[sgcl](../README.md) › [core](README.md)

# sgcl::managed_frame, sgcl::frame_ptr\<Promise\>

```cpp
#include "sgcl/core/coroutine.h"   // or "sgcl/core.h"

namespace sgcl {
    struct managed_frame;

    template<class Promise>
    class frame_ptr;
}
```

The frame of a C++20 coroutine, where its parameters, locals, temporaries and promise live between suspensions, is
allocated with `operator new`: heap memory the collector does not see. A `tracked_ptr` in such a frame breaks rule
1 of [The rules](README.md#the-rules) and its object may be collected under it; debug builds assert it.
`sgcl/core/coroutine.h` is the way out. A promise type that derives from [managed_frame](managed_frame.md) gets its
frames from the managed heap instead, as buffers of words the collector traces conservatively, so everything the
coroutine holds is a root for as long as the frame is held. The frame is held through a
[frame_ptr\<Promise\>](frame_ptr.md): a [root_ptr](root_ptr.md) to the frame and the coroutine handle, move-only,
that destroys the coroutine when destroyed. [generator\<T\>](generator.md) is a coroutine type built this way in the
core; `async::task<T>` of the async module ([task](../async/task.md)) is another, which runs on the
[scheduler](../async/scheduler.md) or by hand.

The buffer is four words longer than the frame, and the frame starts past them: a header that belongs to whoever
drives the coroutine. The core gives it a length and no layout; the async module keeps a task's executor, its
task-locals and the link of an executor's queue there ([executor](../async/executor.md),
[task_local](../async/task_local.md)), and the generator, which runs where it is called, leaves it zero. The handle
addresses the frame past the header, the frame's own pointers the buffer.

The compiler looks the allocation function of a coroutine up in the scope of its promise type, so a promise that
derives from `managed_frame` inherits its `operator new` and `operator delete`. `operator new` allocates the frame
as a managed buffer of words, the size rounded up to whole words and zeroed, in the state of an object a
`unique_ptr` owns: a root already. `get_return_object` of the promise constructs a `frame_ptr` from the coroutine
handle, which takes the frame over into a `root_ptr` and stores the frame's own `tracked_ptr` in the promise
(`managed_frame::self`: what an awaiter copies to hold the frame while the coroutine waits on a channel, on a task or
on the scheduler's queue). From then on the frame is an ordinary managed object, kept by whatever holds the
`frame_ptr` or the waiting coroutine. `operator delete` runs when the coroutine is destroyed, after the destructors
of the coroutine's locals and promise: it frees a frame that no `frame_ptr` took over (an exception thrown before
`get_return_object`, for instance) and does nothing for a frame taken over, whose memory is the collector's once
nothing refers to it.

The collector traces a frame conservatively: every word that holds the address of a managed object keeps that
object, and a word that holds data proves nothing about its offset, since the same offset is a pointer in one frame
and data in another (the pointer maps of managed objects, which narrow by elimination, do not apply). The cost is
the allocation of the frame as a managed buffer, a few tens of nanoseconds instead of `malloc`, and one conservative
pass over the frame's words per cycle; a frame that holds no managed pointers costs that pass and nothing else.

## Rules

- A `frame_ptr` holds a `root_ptr`, and so does everything built on it: a `task`, a `generator`, a coroutine type
  of your own. It lives anywhere: on a stack, in a managed object, in another frame, in a
  `std::vector<async::task<int>>`, in a global. What it costs is a cell per handle ([root_ptr](root_ptr.md)), one per
  coroutine.
- The parameters, locals, temporaries and promise members of a coroutine whose promise derives from `managed_frame`
  are roots while the frame is held: a `tracked_ptr`, a container, a `task` held across a suspension all keep what
  they refer to. A waiting coroutine (on a channel, on a task, on the scheduler's queue) is held by what it waits on,
  so a detached task's frame lives while it runs.
- A promise that does not derive from `managed_frame` lives in `operator new` memory together with the rest of the
  frame, so neither it nor the coroutine's locals or parameters may hold a `tracked_ptr` (rule 1); the collector
  would not see the pointer, and the object could be collected while the coroutine is suspended.
- A `std::coroutine_handle` keeps nothing alive (rule 3). The handle a `frame_ptr` returns is valid while that
  `frame_ptr` holds the frame and no longer: once the `frame_ptr` is destroyed, moved from or `destroy()`ed, the
  frame's memory belongs to the collector. The coroutine is destroyed through the `frame_ptr`, never through the
  handle: the `frame_ptr` would destroy it a second time.
- Destroying a `frame_ptr` destroys the coroutine at once, on the calling thread, which runs the destructors of its
  locals and promise, wherever they were suspended; the frame's memory is reclaimed by a later cycle. A `frame_ptr`
  moved out of a function takes its frame with it.
- A `frame_ptr` has no synchronization of its own: a coroutine is resumed or destroyed by one thread at a time, as
  with a `std::coroutine_handle`, and which thread that is does not matter to the collector (the scheduler's
  workers resume it on whichever is free).

## Example

A coroutine type of your own: the promise derives from `managed_frame`, the object returned holds a `frame_ptr`.
Everything else about the promise is ordinary C++20, `tracked_ptr` members included, since the promise lives in the
frame.

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>

using namespace sgcl;

struct Node {
    int value;
    tracked_ptr<Node> next;
};

class walker {
public:
    struct promise_type : managed_frame {
        tracked_ptr<Node> current;  // in the frame: a root

        walker get_return_object() {
            return walker(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        std::suspend_always yield_value(tracked_ptr<Node> n) noexcept {
            current = n;
            return {};
        }
        void return_void() noexcept {
        }
        void unhandled_exception() {
            throw;  // out of step()
        }
    };

    bool step() {  // to the next co_yield: true, to the end: false
        _frame.resume();
        return !_frame.done();
    }
    const tracked_ptr<Node>& current() const {
        return _frame.promise().current;
    }

private:
    explicit walker(std::coroutine_handle<promise_type> h) : _frame(h) {}  // takes the frame over
    frame_ptr<promise_type> _frame;
};

walker walk(tracked_ptr<Node> head) {  // the parameter: in the frame, a root
    for (auto n = head; n; n = n->next) {
        co_yield n;
    }
}

int main() {
    walker w = walk(make_tracked<Node>(1, make_tracked<Node>(2, make_tracked<Node>(3))));
    collector::force_collect(true);  // optional: the list lives in the frame alone
    while (w.step()) {
        println("{}", w.current()->value);
    }
}
```

Output:

```text
1
2
3
```

## See also

- [managed_frame](managed_frame.md): the base of a promise whose frame is managed
- [frame_ptr](frame_ptr.md): the owner of such a coroutine
- [generator](generator.md): the coroutine type of the core built on a managed frame
- [task](../async/task.md), [generator](../async/generator.md): `task` and `async::generator`, the coroutine types of the async module;
  [scheduler](../async/scheduler.md): what runs the tasks
- [tracked_ptr](tracked_ptr.md), [root_ptr](root_ptr.md), [collector](collector.md)
- [README: Coroutines](../async/README.md#coroutines), [README: The rules](README.md#the-rules)
