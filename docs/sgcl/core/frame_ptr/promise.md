[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::promise

```cpp
Promise& promise() const noexcept;
```

The coroutine's promise, the one in the frame: what a coroutine type reads its results from (a value, an error).
Precondition: the `frame_ptr` is not empty.

## Parameters

None.

## Return value

A reference to the promise, valid while the `frame_ptr` holds the frame.

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
        int result = 0;
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
        void return_value(int v) noexcept {
            result = v;
        }
        void unhandled_exception() noexcept {
        }
    };
    frame_ptr<promise_type> frame;
};

task answer() {
    co_return 42;
}

int main() {
    task t = answer();
    t.frame.resume();
    println("{}", t.frame.promise().result);
}
```

Output:

```text
42
```

## See also

- [handle](handle.md): the coroutine handle
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
