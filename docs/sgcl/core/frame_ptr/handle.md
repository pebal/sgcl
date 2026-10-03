[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::handle

```cpp
handle_type handle() const noexcept;
```

The coroutine handle, null when the `frame_ptr` is empty: for the operations `frame_ptr` does not wrap (`address()`,
passing the handle to an awaiter of your own). The handle addresses the frame four words past the start of its
managed buffer, past the header.

The handle is valid while this `frame_ptr` holds the frame, and keeps nothing alive itself. It is never destroyed
through: the `frame_ptr` destroys the coroutine, and a second destruction would run on a frame that is gone.

## Parameters

None.

## Return value

The handle, or a null handle.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>

using namespace sgcl;

struct task {
    struct promise_type : managed_frame {
        task get_return_object() {
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

task nothing() {
    co_return;
}

int main() {
    task t = nothing();
    std::coroutine_handle<task::promise_type> h = t.frame.handle();
    println("{} {}", h.address() != nullptr, &h.promise() == &t.frame.promise());

    frame_ptr<task::promise_type> empty;
    println("{}", empty.handle() == nullptr);
}
```

Output:

```text
true true
true
```

## See also

- [promise](promise.md): the promise, without the handle
- [release](release.md): the handle, the frame let go
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
