[sgcl](../README.md) › [core](README.md)

# sgcl::frame_ptr\<Promise\>

```cpp
#include "sgcl/core/coroutine.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Promise>
    class frame_ptr;
}
```

`sgcl::frame_ptr<Promise>` is the owner of a coroutine whose promise derives from [managed_frame](managed_frame.md):
two words, a [root_ptr](root_ptr.md) to the frame and the coroutine handle. It is move-only and destroys the
coroutine when destroyed, as a `std::unique_ptr` destroys its object; `release()` lets go of the frame without
destroying the coroutine (a task detached). [generator](generator.md) and `async::task` are each a single
`frame_ptr` and forward to it; a coroutine type of your own holds one the same way ([coroutine](coroutine.md)).

What differs from a `std::coroutine_handle`: the handle is a raw address that keeps nothing alive and is destroyed
by hand; a `frame_ptr` holds the frame by a root, so it lives anywhere and the frame with it, and it destroys the
coroutine once.

## Rules

- A `frame_ptr` holds a `root_ptr`: it lives anywhere, on a stack, in a managed object, in another frame, in a `std`
  container, in a global. What it costs is a cell per handle, one per coroutine.
- The handle it returns is valid while it holds the frame and no longer. The coroutine is destroyed through the
  `frame_ptr`, never through the handle: the `frame_ptr` would destroy it a second time.
- Destroying a `frame_ptr` destroys the coroutine at once, on the calling thread; the frame's memory is reclaimed by
  a later cycle.
- No synchronization of its own: a coroutine is resumed or destroyed by one thread at a time, as with a
  `std::coroutine_handle`.

## Template parameters

| Parameter | Description |
|---|---|
| `Promise` | The promise type of the coroutine; it derives from `managed_frame`. |

## Member types

| Type | Definition |
|---|---|
| `promise_type` | `Promise` |
| `handle_type` | `std::coroutine_handle<Promise>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](frame_ptr/frame_ptr.md) | constructs an empty `frame_ptr`, or takes a coroutine's frame over |
| `(destructor)` | destroys the coroutine, if any ([destroy](frame_ptr/destroy.md)) |
| [operator=](frame_ptr/operator_assign.md) | destroys the coroutine held and takes another's over |

#### Observers

| Function | Description |
|---|---|
| [operator bool](frame_ptr/operator_bool.md) | checks whether there is a coroutine |
| [handle](frame_ptr/handle.md) | the coroutine handle |
| [promise](frame_ptr/promise.md) | the coroutine's promise |
| [done](frame_ptr/done.md) | checks whether the coroutine is at its final suspend point |

#### Modifiers

| Function | Description |
|---|---|
| [resume](frame_ptr/resume.md) | resumes the coroutine |
| [destroy](frame_ptr/destroy.md) | destroys the coroutine and lets go of the frame |
| [release](frame_ptr/release.md) | lets go of the frame without destroying the coroutine |

## Complexity

Every operation is constant, but what the coroutine runs: `resume()` runs it to its next suspension, `destroy()`
runs the destructors of its locals and promise.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>
#include <vector>

using namespace sgcl;

// A coroutine type that is a frame_ptr: each resume is one step
struct steps {
    struct promise_type : managed_frame {
        steps get_return_object() {
            auto h = std::coroutine_handle<promise_type>::from_promise(*this);
            return steps{frame_ptr<promise_type>(h)};
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        std::suspend_always yield_value(int) noexcept {
            return {};
        }
        void return_void() noexcept {
        }
        void unhandled_exception() noexcept {
        }
    };
    frame_ptr<promise_type> frame;
};

steps count(string name, int n) {
    for (int i : range(n)) {
        println("{} {}", name, i);
        co_yield i;
    }
}

int main() {
    std::vector<steps> all;  // a std container: the frame_ptr holds its frame by a root
    all.push_back(count("a", 2));
    all.push_back(count("b", 1));
    collector::force_collect(true);  // optional: the parameters survive in the frames
    for (auto& s : all) {
        while (!s.frame.done()) {
            s.frame.resume();
        }
    }
}
```

Output:

```text
a 0
a 1
b 0
```

## See also

- [managed_frame](managed_frame.md): the base of the promise
- [coroutine](coroutine.md): how a frame becomes managed
- [root_ptr](root_ptr.md): what holds the frame
- [generator](generator.md): a coroutine type built on a `frame_ptr`
