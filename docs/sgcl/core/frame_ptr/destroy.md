[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::destroy

```cpp
void destroy() noexcept;
```

Destroys the coroutine, if any: runs the destructors of its locals and promise wherever it was suspended
(`std::coroutine_handle::destroy`), then lets go of the frame, whose memory the collector reclaims once nothing
refers to it. The `frame_ptr` is empty afterwards. The destructor and the move assignment call it; calling it early
releases what the frame holds before the `frame_ptr` goes out of scope.

## Parameters

None.

## Return value

None.

## Complexity

The destructors of the coroutine's locals and promise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>

using namespace sgcl;

struct Noisy {
    ~Noisy() {
        println("the local destroyed");
    }
};

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

task waiting() {
    Noisy local;
    co_await std::suspend_always();
}

int main() {
    task t = waiting();
    t.frame.resume();  // suspended with its local alive
    t.frame.destroy();
    println("{} {}", bool(t.frame), t.frame.done());
}
```

Output:

```text
the local destroyed
false true
```

## See also

- [release](release.md): lets go of the frame without destroying the coroutine
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
