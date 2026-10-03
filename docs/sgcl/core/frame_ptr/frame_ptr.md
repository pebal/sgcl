[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::frame_ptr

```cpp
frame_ptr() noexcept = default;                // (1)
explicit frame_ptr(handle_type h) noexcept;    // (2)
frame_ptr(frame_ptr&& o) noexcept;             // (3)
frame_ptr(const frame_ptr&) = delete;          // (4)
```

1. An empty `frame_ptr`: `false`, `done()`.
2. Takes the coroutine's frame over: the frame passes from the state `managed_frame::operator new` left it in, owned
   like the object of a `unique_ptr`, to a `root_ptr`, and the promise's `self` is set to the frame. `h` must be the
   handle of a coroutine whose promise derives from `managed_frame`, and no other `frame_ptr` may have taken that
   frame; the place to call it is the promise's `get_return_object`, with
   `std::coroutine_handle<Promise>::from_promise(*this)`.
3. Takes the frame and the handle of `o` over; `o` is empty after.
4. Not copyable: a coroutine has one owner.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle of the coroutine whose frame is taken over |
| `o` | the `frame_ptr` to take the coroutine from |

## Complexity

Constant: (2) takes a root cell for the frame.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>
#include <utility>

using namespace sgcl;

struct task {
    struct promise_type : managed_frame {
        task get_return_object() {
            // the frame_ptr is made here, from the handle of this promise's coroutine
            auto h = std::coroutine_handle<promise_type>::from_promise(*this);
            return task{frame_ptr<promise_type>(h)};
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void return_void() noexcept {
        }
        void unhandled_exception() noexcept {
        }
    };
    frame_ptr<promise_type> frame;
};

task hello() {
    println("hello");
    co_return;
}

int main() {
    frame_ptr<task::promise_type> empty;
    task t = hello();
    frame_ptr<task::promise_type> moved = std::move(t.frame);
    println("{} {} {}", bool(empty), bool(t.frame), bool(moved));
    moved.resume();
}
```

Output:

```text
false false true
hello
```

## See also

- [operator=](operator_assign.md): takes another coroutine over
- [release](release.md): lets go of the frame without destroying the coroutine
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
