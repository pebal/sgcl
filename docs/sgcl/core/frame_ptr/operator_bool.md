[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the `frame_ptr` holds a coroutine. It is empty when default-constructed, moved from, after
[destroy](destroy.md) and after [release](release.md). A coroutine that has run to its end is still held: it is
[done](done.md), not gone.

## Parameters

None.

## Return value

`true` when there is a coroutine, `false` when the `frame_ptr` is empty.

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
    println("{}", bool(t.frame));
    t.frame.resume();  // to its end: done, still held
    println("{} {}", bool(t.frame), t.frame.done());
    t.frame.destroy();
    println("{}", bool(t.frame));
}
```

Output:

```text
true
true true
false
```

## See also

- [done](done.md): checks whether the coroutine has ended
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
