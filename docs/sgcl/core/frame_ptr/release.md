[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](README.md)

# sgcl::frame_ptr\<Promise\>::release

```cpp
[[nodiscard]] handle_type release() noexcept;
```

Lets go of the frame without destroying the coroutine and returns its handle, as `unique_ptr::release` returns the
pointer; the `frame_ptr` is empty afterwards. The coroutine goes on wherever it is and must see to its own end (a
detached task destroys itself at its final suspend). The handle returned keeps nothing alive (rule 3 of
[The rules](../README.md#the-rules)): it stays valid while something else holds the frame, the coroutine's own
scheduler entry for a detached task.

## Parameters

None.

## Return value

The coroutine handle, or a null handle when the `frame_ptr` was empty.

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
        std::suspend_never final_suspend() noexcept {  // destroys itself at its end
            return {};
        }
        void return_void() noexcept {
        }
        void unhandled_exception() noexcept {
        }
    };
    frame_ptr<promise_type> frame;
};

task detached() {
    println("ran to its end");
    co_return;
}

int main() {
    task t = detached();
    auto keep = t.frame.promise().self;  // what holds a detached task's frame: here, a local
    auto h = t.frame.release();  // the frame_ptr empty, the coroutine not destroyed
    println("{} {}", bool(t.frame), bool(h));
    h.resume();
}
```

Output:

```text
false true
ran to its end
```

## See also

- [destroy](destroy.md): destroys the coroutine and lets go of the frame
- [handle](handle.md): the handle, the frame kept
- [sgcl::frame_ptr\<Promise\>](README.md)
