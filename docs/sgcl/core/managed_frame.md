[sgcl](../README.md) › [core](README.md)

# sgcl::managed_frame

```cpp
#include "sgcl/core/coroutine.h"   // or "sgcl/core.h"

namespace sgcl {
    struct managed_frame {
        using word = /* one word of the frame's buffer */;

        static void* operator new(size_t size) noexcept;
        static void operator delete(void* p, size_t) noexcept;

        tracked_ptr<word> self;
    };
}
```

**Requires [rooted](rooted/README.md) outside a stack or a managed object.**

`sgcl::managed_frame` is the base of a promise type whose coroutines get their frames from the managed heap. The
compiler looks the allocation function of a coroutine up in the scope of its promise type, so a promise that derives
from `managed_frame` inherits its `operator new` and `operator delete`, and every frame of such a coroutine is a
managed buffer of words the collector traces conservatively: the coroutine's parameters, locals, temporaries and
promise are roots for as long as the frame is held ([coroutine](coroutine.md)). Neither operator is called by hand:
the compiler calls them for every coroutine of the promise, with the size of the whole frame.

A promise of your own derives from `managed_frame` and returns, from `get_return_object`, an object that holds a
[frame_ptr](frame_ptr/README.md) made from the handle. Everything else about the promise is ordinary C++20:
`initial_suspend`, `final_suspend`, `return_value` or `return_void`, `yield_value`, `unhandled_exception`, and any
members it needs, `tracked_ptr` members included, since the promise lives in the frame. Neither `std` nor Go has a
counterpart: a C++ frame is the program's to allocate, a Go goroutine's stack is the collector's from the start.

## Rules

- The frame is allocated as a managed buffer: the size rounded up to whole words, four words of header in front of
  it, all zeroed, in the state of an object a `unique_ptr` owns until a `frame_ptr` takes it over. Out of managed
  memory, the program ends with a diagnostic, as every managed allocation does
  ([collector](collector/README.md#the-memory-limit)).
- `operator delete` runs when the coroutine is destroyed, after the destructors of its locals and promise. It frees
  a frame that no `frame_ptr` took over (an exception thrown before `get_return_object`) and does nothing for a frame
  taken over, which the collector reclaims once nothing refers to it.
- `self` is set by the `frame_ptr` that takes the frame over: the frame's own tracked pointer, a cycle of one that
  holds nothing alive. An awaiter copies it to hold the frame while the coroutine waits.
- A coroutine that waits on the library's primitives (a channel, a task, the scheduler) must have a promise derived
  from `managed_frame`: the wait asks for `self`, and a promise without it does not compile.

## Member types

| Type | Definition |
|---|---|
| `word` | one word of the frame's buffer, a class of the library: the type `self` points at, by which the library's queues and waiters hold a frame |

## Member objects

| Member | Description |
|---|---|
| `self` | the frame's own tracked pointer, `tracked_ptr<word>`, to the first word of its buffer; null until a `frame_ptr` takes the frame over |

## Member functions

| Function | Description |
|---|---|
| `operator new` | `static`, `noexcept`: allocates a frame of `size` bytes as a managed buffer of words, past four words of header |
| `operator delete` | `static`, `noexcept`: frees a frame no `frame_ptr` took over, does nothing for one taken over |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>
#include <type_traits>

using namespace sgcl;

// The smallest coroutine type on a managed frame: run once, keep a value in the promise
struct once {
    struct promise_type : managed_frame {
        tracked_ptr<int> kept;  // a member of the promise: in the frame, a root

        once get_return_object() {
            auto h = std::coroutine_handle<promise_type>::from_promise(*this);
            return once{frame_ptr<promise_type>(h)};
        }
        std::suspend_never initial_suspend() noexcept {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void return_value(tracked_ptr<int> v) noexcept {
            kept = v;
        }
        void unhandled_exception() noexcept {
        }
    };
    frame_ptr<promise_type> frame;
};

once make(int n) {
    co_return make_tracked<int>(n);
}

int main() {
    once o = make(7);
    collector::force_collect(true);  // optional: the int is held by the frame alone
    println("{} {}", *o.frame.promise().kept, std::is_base_of_v<managed_frame, once::promise_type>);
}
```

Output:

```text
7 true
```

## See also

- [coroutine](coroutine.md): how a frame becomes managed, a coroutine type of your own
- [frame_ptr](frame_ptr/README.md): the owner of the frame
- [generator](generator/README.md): the coroutine type of the core built on it
